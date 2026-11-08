# KDE Plasma Windows 原生移植

目标是交付可放入 Windows x64 虚拟机验证的原生 Plasma 桌面与 Dolphin 运行包。Plasma 主体与 Dolphin 均已通过 MSVC 编译，VM 测试 ZIP 已生成，位于 `dist/20261108-195709/plasma-windows-vm.zip`。尚未进行虚拟机运行验收，不能据此认定整个 KDE 已完成移植。

## 版本与构建

- Qt 6.11.1、KDE Frameworks 6.28.0、Plasma 6.7.4、Dolphin/KIO Extras 26.04.3。
- MSVC 2022 v143 x64；源码和脚本位于本目录，NTFS 构建根为 `D:/KDE/CraftRoot`。
- `build-lock.json` 固定移植仓库、Craft、blueprints 和 Git 源码版本；官方发布包哈希及补丁验证记录见 `source-verification.json` 与 `extra-source-verification.json`。
- `configure-port.py` 生成 Windows 配方覆盖；`build-port.ps1` 编译安装各组件、原生插件并调用 `package-port.ps1`。
- 详细构建日志位于 `logs/`，流水线输出为 `build-port.log`。

## 已编译的组件

Dolphin、KIO Extras、Plasma Workspace（含 plasmashell、KRunner）、Plasma Desktop、libplasma、Plasma5Support、活动库、活动统计、带 SQLite 资源评分插件的活动管理服务，以及修改过的 KDE Frameworks 已编译安装。

原生扩展已编译：Win32/DWM 窗口后端、托盘桥接、Windows 开始菜单 QML 模块、Windows 菜单/音量/输入法小部件、标题栏工具、shellswitch 和窗口后端验收探针。

Windows 使用 DWM。构建排除了 X11/Wayland 专属实现，保留 Windows 原生窗口和工作区处理；KWin 仅提供由官方源码生成的 D-Bus 接口元数据。KIO 启用会话 D-Bus，为活动服务和最近使用文件提供 KDirNotify。

## 源码修复与验证

- 重新生成原仓库中格式损坏的补丁，官方发布源码上的补丁链已严格验证；当前 37 个补丁文件通过格式检查。
- 补全 DWM 缩略图声明，修复 Plasma Shell、KRunner、通知视图及启动画面的平台条件。
- 为 Windows 活动 worker 补齐依赖查找，启用活动数据库插件。
- `native/CMakeLists.txt` 将原生扩展纳入统一构建安装，启动脚本在生成服务缓存前索引 Windows 开始菜单。
- `verify-runtime-imports.py` 检查运行入口、所有 QML/插件 DLL 及其普通 PE 导入依赖的 x64 架构和文件完整性。此项为静态检查，不验证导出符号、延迟导入、QML 运行逻辑或 VM 上的系统 API 兼容性。
- 首次启动的默认面板加载 Windows 原生菜单、音量和输入法小部件；补装 Breeze 在 Windows 默认跳过的 Next 壁纸资源。
- 静态检查已通过：603 个 x64 模块，无缺失的普通导入依赖。
- 打包复制运行目录，排除 PDB、静态库和对象文件，生成 ZIP 与 SHA256。

## VM 测试

1. 将最终 ZIP 内容解压到 VM 的 `D:/KDE/CraftRoot`。也可解压到 `C:/KDE/CraftRoot`，此时 D: 必须空闲，脚本会通过 SUBST 映射编译前缀。
2. 运行 `deploy-vm.cmd`，不带参数启动临时 Plasma 会话。
3. 检查桌面、面板、启动菜单、Windows 应用启动、窗口任务栏、托盘、音量、中文输入法、多屏、Dolphin 文件操作及退出。
4. 可运行 `run-vm-probe.cmd`，结果保存到 `bin/kws-probe-result.txt`；Plasma 日志位于 `%TEMP%/plasmashell-debug.log`。
5. `deploy-vm.cmd install` 设置当前用户登录 Shell，`deploy-vm.cmd restore` 恢复备份；先完成临时会话验收再测试登录启动。

构建过程中未在宿主机启动 Plasma 桌面或修改登录 Shell。Dolphin 未启用 Baloo 全盘索引及 PackageKit 软件安装集成。Linux 特有服务与全部 KDE 应用不属于本次运行包已验证的功能；电源管理、全局快捷键、通知及应用集成等仍需 VM 实测。

## VM 首次反馈修复：qml-fix1

VM 日志显示 Desktop.qml 缺少四个 QML 模块。已在旧包复现：Qt 默认搜索 bin/qml，而模块在根目录 qml。新增 bin/qt.conf 和启动脚本的 QML_IMPORT_PATH/QML2_IMPORT_PATH 后，无窗口导入探针通过。修补包：dist/20261108-qml-fix1/plasma-windows-qml-fix1.zip，覆盖原 CraftRoot。菜单桥接脚本同时修复 Desktop Entry 值中的反斜杠转义。此修复仅通过模块加载检测，桌面继续启动情况待 VM 复测；kglobalaccel 服务仍未提供。

## VM 后续修复：runtime-fix2

恢复被禁用的 desktop KIO worker 和 desktopnotifier，新增 Quick Charts 运行依赖。覆盖补丁位于 dist/20261108-runtime-fix2/plasma-windows-runtime-fix2.zip，包含前一版 QML 路径修复。旧运行目录覆盖后，desktop:/ 只读目录列举及 Quick Charts 对象实例化探针通过，609 个 x64 模块静态导入无缺失。完整 VM 桌面与通知交互待复测。

## 显示修复：display-fix3

开始菜单采用 Plasma 控件样式与实色背景，自适应网格、系统字体、两行名称和悬停高亮；目录采用普通文件夹图标。新增面板默认不透明，已有面板通过 apply-panel-opacity.cmd 在运行会话中设置并保存。此脚本不终止 Explorer。修补包包含前两版修复，位于 dist/20261108-display-fix3/plasma-windows-display-fix3.zip。菜单 QML 已通过真实 Qt 引擎编译检查，VM 视觉验收待进行。

交付约定：用户每次恢复到仅安装原始运行包、未应用补丁的 VM 快照；后续每个修补包都必须累计包含此前全部修复，不能依赖已安装的旧补丁。display-fix3 已核对包含 fix1/fix2 的全部运行文件。

## 图标修复：icons-fix4

累计补丁包含第 1–3 版全部文件，更新 Windows 原生图标提供器及桌面图标委托；Shell 文件关联与 QImage::fromHICON 处理透明蒙版，保留图片缩略图路径。文件夹/ZIP/快捷方式检测均获得可见像素，桌面 QML 编译通过。VM 视觉验收待复测。

## 累计修复 fix5 与源码发布

桌面 Windows 快捷方式使用原始 localPath 调用 ShellExecuteEx，避免将盘符当作 URL 协议，并保留快捷方式参数及工作目录。FolderModel 实际调用测试已通过。桌面及启动菜单图标改用 Breeze 栅格提供器。面板增加仅针对 Explorer 任务栏的无焦点层级恢复，桌面背景不置顶；MSVC 编译通过，VM 层级效果待验收。新增 make-cumulative-patch.py 和固定清单，直接覆盖原始运行包。项目新增独立代码使用 GPL-3.0-or-later，上游代码保留原许可。
