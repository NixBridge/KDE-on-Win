# Fix17：弹窗快速接管与 KDE 应用选择器

本包覆盖原始 VM 包，包含此前全部补丁。重启 VM，覆盖 CraftRoot 后运行 `deploy-vm.cmd`。完整支持范围见 [STYLE_SCOPE.md](STYLE_SCOPE.md)。

## 标题栏响应

过去新弹窗依赖 1500 ms 扫描发现。现在监听原生 SHOW、DIALOGSTART 和前台事件，排队接管具体窗口，并在几何尚未稳定时以 25 ms 间隔有限重试。销毁事件会清理对象，避免等待下一轮扫描。扫描仍作为兜底。跨进程事件和渲染调度意味着不承诺第一帧完全没有原生标题栏。

普通权限服务仍不接管更高权限进程。对于以管理员运行的 Setup，可在 VM 运行 `start-window-decorations-admin.cmd`，接受同一账户的 UAC 提升；新服务通过 D-Bus 请求旧服务退出并接替它。此入口不接管 UAC 安全桌面。

隐藏测试窗口验证不依赖扫描的事件接管路径、重复事件合并，以及此前的对比度、最大化、最小化和自绘排除功能。

## KDE 应用选择器

新增中文/英文 Qt/KDE 应用选择窗口，支持搜索 Windows 已注册的文件/协议处理器、打开一次、在 KDE 中记住处理器、选择其他程序。通过 Windows `IAssocHandler` 和 `IDataObject` 启动所选程序，保留原生注册信息；选择器自身从候选中排除，避免递归。

安装器加入文件右键“Open with KDE...”和 mailto 的后备关联，关联版本升级为 3。已有受保护 `UserChoice` 保持不变。若 mailto 仍出现图二的 Windows 选择窗口，运行 `configure-kde-link-chooser.cmd`，在系统默认应用页面将 MAILTO 选为 KDE Application Chooser；也可从项目系统设置的“关于与诊断”进入。Windows 必须由用户确认的那一步仍是系统界面。本补丁没有给 Windows 原生 XAML 选择器本体换肤。

直接使用：`bin\plasma-open-with.exe "mailto:someone@example.com"` 或传入文件路径；`bin\plasma-kde-launcher.exe open-with <目标>` 也可调用。“在 KDE 中记住”只保存选择器内部的偏好，不伪造系统默认应用记录。

默认应用入口使用微软支持的 [Default Apps URI](https://learn.microsoft.com/en-us/windows/apps/develop/launch/launch-default-apps-settings)，按当前用户的注册应用名称定位。

自动测试只枚举处理器，不调用真实应用；COM 分发测试将私有临时文件的数据对象交给测试自建处理器，验证只调用一次且保留 Unicode 路径。

## 尚未完成

真正的 Windows 密码登录界面、第三方客户区和所有系统弹窗的全局 KDE 换肤尚未实现。`deploy-vm.cmd install` 已支持的是验证成功后的 Plasma Shell，不是密码输入页面。见范围文档中的明确区分。

VM 复测：打开 Setup 的退出确认框，比较标题接管延迟；测试普通与管理员程序；配置 MAILTO 后点击邮件链接，选择处理器并发送至应用；复测记住选择、取消、没有处理器与文件右键入口。
