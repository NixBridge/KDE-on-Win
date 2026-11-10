# Fix25 启动热修复

用户 VM 日志确认文件关联安装器 File.Replace 抛出路径格式错误。Windows PowerShell 5.1 将第三个字符串参数 $null 转为空字符串；改用 [NullString]::Value，保持先写备份再改关联的原子替换顺序。不删除原有备份。

此异常发生在后台关联安装器，不能据此认定是 plasmashell 未显示的直接原因。原始日志没有 plasmashell 输出；新增 deploy-vm 前台进程日志 `%LOCALAPPDATA%\plasma-win-session\logs\plasmashell.exe.log`。配置 KDE_ON_WIN_SETTINGS 时使用该文件旁 logs 目录。失败时显示日志与原始退出码，日志不可写时回退到控制台。

验证：Windows PowerShell 5.1 真实重复文件替换；whoami/where 安全替身的输出捕获与退出码；特殊字符路径；不可用日志目录回退；关联计划、路径搬移、累计清单与脚本解析回归。脚本修改不涉及原生组件，无需重新编译。未运行真实 Plasma、未修改本机 Shell 或关联，未完成 VM 验收。

关闭 VM 内 KDE 组件，将 20261110-hotfix1-fix25 累计 ZIP 覆盖原运行目录，运行 deploy-vm.cmd。保留 Fix1–Fix25 文件。如果仍未出现桌面，请提供新产生的 plasmashell.exe.log 和终端退出码；任务栏修复的“Plasma session not available”属于会话尚不可用的表现，不能单独用于确定根因。

## Hotfix2：早期退出可诊断

用户补充：运行后闪退，没有 plasmashell.exe.log，仅标题栏装饰生效。说明至少装饰助手已运行，不能认定 Plasma 会话已启动。本轮确认并修复的是日志覆盖过晚的缺口，不声称已复现或修复 VM 闪退根因。

普通 deploy-vm.cmd 启动自动通过独立子 cmd 捕获整个启动链，在运行目录写 startup-diagnostic.log。解析错误、普通 EXIT、初始化失败与原始退出码均保留；失败窗口暂停。目录不可写时回退 TEMP 下 kde-startup-diagnostic.log。install/restore 保持原逻辑，不启用 Shell 替换。子进程使用 pushd 后的固定脚本名，避免特殊路径拼入双层 cmd 引号。可直接运行 diagnose-startup.cmd；测试用 --no-pause，自动化可设 KDE_BOOTSTRAP_NO_PAUSE=1。

回归覆盖真实启动入口、环境、菜单生成与服务调度；私有测试跳过 D-Bus，用 whoami 副本替代服务/桌面 EXE。验证正常链到桌面调用、菜单失败、普通 EXIT、特殊字符路径与输出/退出码保留。未启动本机 Plasma、未修改 Shell 或关联。另实际运行 plasmashell --version 得到 6.7.4、退出码 0，仅证明版本命令可加载。仅改脚本，不需原生重编译。

未再次要求用户操作 VM；替身测试不是 VM 验收。真实 VM 闪退原因仍未确认，需完整启动输出或可用的非交互来宾执行通道。交付为 20261110-hotfix2-fix25 全量累计包，保留历史内容。

Hotfix2 验证结果：现有完整回归共 66 项全部通过；未重编译原生组件，未进行真实 VM 交互验收，启动根因仍未确认。
