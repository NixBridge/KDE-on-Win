# 实验性 KDE 登录磁贴

本模块是真正的 Windows V2 凭据提供程序，会出现在 Windows 安全登录桌面的“登录选项”中；不是登录后的欢迎页。它显示 KDE 图案、项目标题和本地账户密码字段，由 Windows 负责呈现安全输入框并完成认证。

**它没有把整个 LogonUI 或锁屏页面替换为 SDDM。** 当前仅枚举本机本地账户；微软账户、Entra/域账户、PIN、Hello、CredUI/UAC、密码修改和旧版单独解锁场景不由此模块接管。

## 在快照 VM 中验证

1. 覆盖累计包后运行 `bin\kde-logon-probe.exe`，应输出 PASS。它只测试合成数据，不验证真实账户。
2. 运行 `install-kde-logon.cmd`，在 VM 接受管理员授权。安装器将 DLL 复制到 Program Files 的受保护目录，再注册单独的项目 CLSID。
3. 注销或重启，在本地账户的“登录选项”选择 KDE 项目磁贴，验证密码登录。
4. 验证失败密码提示、切换回系统密码/PIN选项，并测试重新登录。实际登录尚未由宿主机测试确认。
5. 运行 `restore-kde-logon.cmd` 可注销本项目的磁贴。DLL 可能仍被系统使用，因此恢复入口保留受保护的文件，不强制删除。

安装器不注册 Credential Provider Filter，不禁用系统提供程序，不设置默认提供程序，也不修改 Winlogon 的 Shell。用户主动选择 KDE 磁贴。原有系统登录方式保留。

若希望验证完成后直接进入 Plasma，另行在 VM 运行 `deploy-vm.cmd install`。这是当前用户的会话外壳设置，与密码收集和认证分开。

## 实现与边界

- 账户由 Windows `ICredentialProviderUserArray` 提供，以 SID 绑定本地账户。用户名只读。
- 密码仅暂存在提供程序内存，交由 `CredPackAuthenticationBufferW` 保护并打包，再提交系统 Negotiate 包；没有自定义密码验证、网络发送或密码日志。
- 取消、提交和对象销毁会清理模块持有的密码缓冲区。认证仍由 Windows 执行。
- 安装目录和 DLL 由 Administrators/SYSTEM 写入，Users 仅可读/执行；不从用户可写的 CraftRoot 注册 SYSTEM 加载的 DLL。拒绝部署路径的重解析点。
- 单元测试覆盖合成凭据的保护与解包、未选择默认账户以及不支持场景的拒绝。它不等于实际登录验收。

依据：[Windows 凭据提供程序](https://learn.microsoft.com/en-us/windows/win32/secauthn/credential-providers-in-windows)、[V2 登录接口示例与限制](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/CredentialProvider)、[CredPackAuthenticationBufferW](https://learn.microsoft.com/en-us/windows/win32/api/wincred/nf-wincred-credpackauthenticationbufferw)。
