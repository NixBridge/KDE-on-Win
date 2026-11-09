# KDE 风格接管范围

| 界面 | 当前实现 |
| --- | --- |
| Plasma 桌面、面板、菜单 | 原生 Plasma 与 Windows 后端 |
| 标准 Windows 标题栏 | KDE 绘制层；显示/对话框事件触发快速接管，扫描兜底 |
| 管理员程序的标准标题栏 | 可在 VM 手动运行 `start-window-decorations-admin.cmd`，经 UAC 授权后以同一账户提升服务 |
| 系统设置、编辑器、进程管理、归档、终端、文件管理 | 已提供 KDE 风格程序或原生 KDE 应用 |
| 文件/链接应用选择 | 新增 `plasma-open-with.exe`；系统文件右键的“Open with KDE...”入口及 mailto 处理器 |
| Windows 自带 XAML“选择应用”窗口 | 本体没有换肤。通过默认关联进入 KDE 选择器，仍需遵守 Windows 的默认应用确认机制 |
| 第三方程序客户区、自绘标题栏 | 不受 Qt/KDE 样式控制，本次未做全局换肤 |
| 密码、PIN、Windows Hello、UAC 安全桌面 | **尚未替换为 KDE**；当前项目没有实现凭据提供程序，也没有更改 Windows 安全登录组件 |
| 登录成功后的桌面会话 | 已有 `deploy-vm.cmd install` 可设置当前用户的 Plasma 会话外壳；这不替换密码输入界面 |

## 真正登录界面的限制

Windows 的密码输入发生在 Winlogon/LogonUI 管理的安全桌面，普通 Plasma 进程不能绘制到该桌面。官方扩展方式是实现凭据提供程序，由 LogonUI 枚举并呈现其磁贴和字段；这不是给整个 LogonUI 加载 Qt 或 SDDM 主题的接口。因此，本次没有把登录后的欢迎窗口当作密码登录替换，也没有声称已实现 SDDM。

实现真正的 Windows 登录扩展需要独立开发和在 VM 中验证账户类型、锁屏/解锁、密码/PIN/Hello 与恢复路径。本补丁没有完成这项工作。微软也建议保留至少一个系统凭据提供程序，并不建议包装系统提供程序。

依据：[凭据提供程序机制及建议](https://learn.microsoft.com/en-us/windows/win32/secauthn/credential-providers-in-windows)、[Winlogon 与凭据提供程序](https://learn.microsoft.com/en-us/windows/win32/secauthn/winlogon-and-credential-providers)。
