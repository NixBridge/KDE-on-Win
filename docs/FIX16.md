# Fix16：统一文件夹打开方式

这是覆盖原始 VM 包的累计补丁，包含 fix1–fix15 的全部文件。重启虚拟机，覆盖 CraftRoot 后运行 `deploy-vm.cmd`。启动时会自动升级当前用户的文件夹关联。已有会话也可手动运行 `install-file-associations.cmd`，等待安装结束。

## 统一入口

- `Directory`、`Drive`、通用 `Folder` 的默认操作，以及显式 `open`、`explore`、`opennewwindow`、`opennewprocess`，统一进入文件夹启动器；覆盖这些操作继承的 Explorer COM 委托。
- Plasma 桌面的文件夹和文件夹快捷方式直接交给启动器，不再依赖 KIO 临时选择应用。开始菜单中的目录快捷方式、无附加参数的文件资源管理器快捷方式也进入 Dolphin。
- 桌面、文档、下载等能够解析为实际文件系统位置的 Windows Shell 路径，先解析实际目录再启动 Dolphin；保留重定向位置、Unicode、空格、驱动器根目录和可访问的 UNC 路径。
- 启动器解析目录 `.lnk` 的真实目标。应用程序快捷方式仍保留原来的执行方式和参数。
- 从 Explorer 等外部程序进入时，启动器补齐项目程序路径和会话 D-Bus 地址，让 Dolphin 使用同一套运行环境。

控制面板、库等没有实际目录的 Windows 虚拟页面不是普通文件夹，仍由 Windows 处理，避免发送 Dolphin 不支持的路径。显式执行 `explorer.exe` 的第三方程序、Windows 自有文件选择对话框也不在文件夹关联的接管范围内。此补丁没有替换或终止 Explorer 进程。

## 安装与恢复

只写当前用户的 `Software\Classes` 文件夹操作，不更改文件扩展名的受保护 `UserChoice`。原值在每次写入前备份至 `%LOCALAPPDATA%\plasma-win-session\file-associations-backup.json`。运行 `install-file-associations.cmd -Restore` 可恢复仍由本项目管理的值；安装后其他程序改动的值不会被覆盖恢复。关联版本升级为 2，使原有 VM 的旧安装标记不会跳过本次更新。

## 验证

启动器的 `--folder-plan` 只解析、不启动文件管理器。自动测试覆盖实际目录、目录快捷方式、名为 `.lnk` 的真实目录、文件 URL、桌面/文档别名、驱动器根目录、无效路径和虚拟页面回退；关联计划测试覆盖三类文件夹和五种操作的命令行及委托设置。宿主机测试不修改关联，也不打开 Dolphin。

VM 复测：分别双击桌面文件夹、文件夹快捷方式、磁盘、文档/下载；在其他应用中选择“打开所在文件夹”；验证均进入 Dolphin。再测试中文和空格目录、重定向桌面、网络共享及关联恢复。

实现依据：[Windows Shell 操作与默认命令](https://learn.microsoft.com/en-us/windows/win32/shell/launch)、[Shell 路径解析接口](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-shcreateitemfromparsingname)。
