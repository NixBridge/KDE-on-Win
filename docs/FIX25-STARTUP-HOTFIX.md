# Fix25 启动热修复

用户 VM 日志确认文件关联安装器 File.Replace 抛出路径格式错误。Windows PowerShell 5.1 将第三个字符串参数 $null 转为空字符串；改用 [NullString]::Value，保持先写备份再改关联的原子替换顺序。不删除原有备份。

此异常发生在后台关联安装器，不能据此认定是 plasmashell 未显示的直接原因。原始日志没有 plasmashell 输出；新增 deploy-vm 前台进程日志 `%LOCALAPPDATA%\plasma-win-session\logs\plasmashell.exe.log`。配置 KDE_ON_WIN_SETTINGS 时使用该文件旁 logs 目录。失败时显示日志与原始退出码，日志不可写时回退到控制台。

验证：Windows PowerShell 5.1 真实重复文件替换；whoami/where 安全替身的输出捕获与退出码；特殊字符路径；不可用日志目录回退；关联计划、路径搬移、累计清单与脚本解析回归。脚本修改不涉及原生组件，无需重新编译。未运行真实 Plasma、未修改本机 Shell 或关联，未完成 VM 验收。

关闭 VM 内 KDE 组件，将 20261110-hotfix1-fix25 累计 ZIP 覆盖原运行目录，运行 deploy-vm.cmd。保留 Fix1–Fix25 文件。如果仍未出现桌面，请提供新产生的 plasmashell.exe.log 和终端退出码；任务栏修复的“Plasma session not available”属于会话尚不可用的表现，不能单独用于确定根因。
