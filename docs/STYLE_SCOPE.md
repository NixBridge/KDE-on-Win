# KDE 风格接管范围

| 界面 | 当前实现 |
| --- | --- |
| Plasma 桌面、面板、菜单 | 原生 Plasma 与 Windows 后端 |
| 标准 Windows 标题栏 | KDE 绘制层；显示/对话框事件触发快速接管，扫描兜底 |
| 管理员程序的标准标题栏 | 可在 VM 手动运行 `start-window-decorations-admin.cmd`，经 UAC 授权后以同一账户提升服务 |
| 系统设置、编辑器、进程管理、归档、终端、文件管理 | 已提供 KDE 风格程序或原生 KDE 应用 |
| 文件/链接应用选择 | 新增 `plasma-open-with.exe`；系统文件右键的“Open with KDE...”入口及 mailto 处理器 |
| Windows 自带 XAML“选择应用”窗口 | 本体没有换肤。通过默认关联进入 KDE 选择器，仍需遵守 Windows 的默认应用确认机制 |
| 第三方传统 Win32 控件 | 新增实验性 32/64 位绘制模块，可单独启用；覆盖普通按钮、选框、输入框配色及列表/树配色 |
| 第三方自绘客户区、WinUI/XAML | 未实现通用替换；自绘标题栏继续保留 |
| 本地账户密码登录 | 新增实验性 V2 凭据提供程序，尚待 VM 实际登录验收；外层 LogonUI 仍由 Windows 呈现 |
| PIN、Windows Hello、UAC 安全桌面 | 不接管，保留系统提供程序 |
| 登录成功后的桌面会话 | 已有 `deploy-vm.cmd install` 可设置当前用户的 Plasma 会话外壳；这不替换密码输入界面 |

## 真正登录界面的限制

Windows 的密码输入发生在 Winlogon/LogonUI 管理的安全桌面，普通 Plasma 进程不能绘制到该桌面。官方扩展方式是实现凭据提供程序，由 LogonUI 枚举并呈现其磁贴和字段；这不是给整个 LogonUI 加载 Qt 或 SDDM 主题的接口。因此，本次没有把登录后的欢迎窗口当作密码登录替换，也没有声称已实现 SDDM。

项目现有独立的本地账户实验模块，见 [LOCAL_LOGON.md](LOCAL_LOGON.md)。仅构建和合成数据测试通过，真实登录、账户兼容性与恢复仍需 VM 验证；没有实现整个安全桌面的主题替换。微软建议保留系统提供程序，本实现没有过滤或禁用它们。

依据：[凭据提供程序机制及建议](https://learn.microsoft.com/en-us/windows/win32/secauthn/credential-providers-in-windows)、[Winlogon 与凭据提供程序](https://learn.microsoft.com/en-us/windows/win32/secauthn/winlogon-and-credential-providers)。
