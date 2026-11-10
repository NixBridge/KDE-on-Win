# Fix25 Hotfix4：历史对照与实际交付目录排查

本轮基线为 Fix20 af1129a、Fix21 04bf5eb、Fix22 4a4ef01、Fix23 ac0c243、Fix24 644ac9d、Fix25 151b114，以及前三轮诊断热修复至 105a14f。没有要求用户继续运行 VM 测试。

## 已确认并修复

### 面板启动空指针

逐段核对 PanelView 的 QML 加载、状态、焦点、浮动动画及窗口层级路径，发现两处未经检查的 rootObject() 解引用。这两处风险在早期代码中已存在，不能声称是 Fix25 新增；新的状态/加载时序可以暴露已有缺陷。

- updateFloatingAnimationDuration 在 QML 根对象为空时读取 property。真实 PanelView 的隔离测试中，未修复代码触发 0xC0000005；加入检查后退出 0。
- refreshStatus 在已有 containment、根对象尚未就绪时处理 AcceptingInputStatus，会调用空根对象的 nextItemInFocusChain。单独保留这条旧路径时，在 Qt6Quick.dll 内复现 0xC0000005；修复后通过。
- 缺少 containment 的状态回调也安全返回。QML 就绪后重新处理待完成的输入状态，避免永久丢失焦点请求；真实 PanelView 与最小 QML 根对象回归验证了就绪后的实际处理，而不是仅验证不崩溃。
- Win32 面板层级与工作区更新现在检查实际 QPA 平台，避免 offscreen/minimal 后端把虚拟窗口 ID 当 HWND，或修改真实系统工作区。

增加 --windows-startup-self-test：强制 offscreen、软件渲染、Qt test-mode 配置及不可连接的私有总线地址；测试真实 PanelView、未就绪/就绪切换以及实际交付 Panel.qml 的编译。不启动可见桌面，不进行登录、Shell 或关联安装。

### 累计补丁遗漏原生构建产物

首次把最初完整 ZIP 与 Hotfix3 累计 ZIP 合并成独立目录，逐项比对实际二进制，而非继续仅使用完整 SDK 运行库。确认下列 8 项未被累计清单覆盖：

| 文件 | 实际影响 |
| --- | --- |
| plugins/plasma/applets/org.kde.plasma.imewin.dll | 已更新的输入布局/目标窗口逻辑没有覆盖到原始包 |
| plugins/plasma/applets/org.kde.plasma.volumewin.dll | 已修复的 COM 初始化/反初始化及图标状态逻辑没有覆盖到原始包 |
| plugins/plasma/applets/org.kde.plasma.windowsmenu.dll | 继续使用旧的原生启动器插件及元数据 |
| bin/trayhost.exe | 继续使用旧托盘桥；其自测在合并目录超时，更新后同测试通过 |
| bin/shellswitch.exe | Shell 切换/恢复的历史修复没有实际覆盖到原始程序 |
| bin/titlebar.exe | 旧的独立标题栏程序未覆盖 |
| bin/kwindowsystem-probe.exe | 窗口后端探针未更新 |
| bin/desktop-launch-probe.exe | 清单缺项；本次比较中原始副本与 SDK 相同，仍纳入完整构建产物覆盖 |

其中前 7 项在实际合并包与 SDK 中 SHA256 不同。8 项均已补入累计覆盖。对原生构建 install_manifest.txt 中的 44 个 EXE/DLL 逐项验证，现已全部列入且合并目录与 SDK 字节一致。

打包工具新增 --native-install-manifest 参数，发现漏发或运行库之外的原生构建产物时，在版本写入及 ZIP 创建前失败。新增真实失败路径回归；原有断言和超时要求没有降低。

## 历史改动核对范围

| 路径 | 对照结论 |
| --- | --- |
| deploy-vm / plasma-common / session-shell / plasma-shell / shell-registry / switch-shell / 菜单及总线配置 | 保留可撤销安装、路径处理、总线失败传播和已有诊断。截图已证明脚本最终调用了桌面进程，不把后台关联异常当成当前访问冲突的直接原因 |
| KWindowSystem Windows 公共/私有接口、WindowList | 核对 QObject/QPointer 所有权、GUI 线程条件、事件过滤、Hook 释放、合并刷新；本轮未回滚或猜测性修改 |
| WindowsWindowTasksModel / WindowsWindowActions / WindowsDesktopApps | 核对构造/延迟初始填充、PID/HWND 身份、插入删除、图标与路径缓存、角色和操作参数；保留历史安全检查与分组 |
| PanelView / DesktopView / 屏幕顺序 / 全屏层级 | 修复上述可复现空根对象风险及非原生 QPA 的 Win32 调用；其余历史层级策略保留 |
| plasmawindowsstyle | 对比 Fix20 后变更，新增装饰白名单不含 plasmashell；QApplication 配色路径与 QWidget 标题栏路径分别核对，没有把所有 Qt 程序粗暴禁用样式 |
| 会话宿主、样式宿主与经典控件 Hook | 对照句柄继承、参数、COM/窗口过程和清理改动，未扩大注入或高风险系统集成范围 |
| 输入法、音量、开始菜单 | 核对控制器与近期差异，重点修复“源代码/SDK 已修，交付插件未更新”的遗漏 |

这是项目关键启动路径及历史改动的代码核对，不等于逐行审计整个 Qt/KDE 上游。已停用/默认未启用的实验性登录组件不是本次启动修复对象；没有修改受保护系统文件或凭据提供程序。

## 验证与边界

最初完整包共 46,625 个条目。合并 Hotfix3 后的关键 QtCore/Gui/Widgets/Qml/Quick、Plasma、KConfig/KIO/KWindowSystem、taskmanager 与样式 DLL 均与 SDK 一致；全部 DLL 比较发现 3 个上述漏发插件，EXE 比较发现 4 个上述旧程序，已经补齐。合并运行库的 646 个 PE 导入检查无缺失，新增依赖闭包检查没有缺少的新 DLL。

未修复的两个空指针路径分别在真实 PanelView 状态测试中复现访问冲突；修复后在 SDK 与独立交付目录都通过，并编译了实际交付 Panel.qml。测试不会显示桌面。第一次合并目录完整回归在旧 trayhost 自测超时中止，该失败没有标为通过；更新漏发二进制后重新执行完整回归。

没有真实 VM 的故障模块/调用栈，不能仅凭相同异常码证明用户 VM 唯一根因就是这两条空指针路径。交付遗漏和可复现崩溃均是确认缺陷，已经实际修复；真实 VM 交互仍未验收。未重置用户主题、桌面配置或缓存。

最终验证：实际合并运行库 69 项完整回归全部通过；x64 plasmashell 已重新编译；3 个 KWindowSystem / 24 个 Workspace 补丁严格重放并对比实际构建源码。交付为 20261110-hotfix4-fix25，全量覆盖而非逐个安装历史修复。真实 VM 交互仍未验收。
