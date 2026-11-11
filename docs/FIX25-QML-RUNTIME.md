# Fix25 Hotfix5：桌面 Qt QML 模块缺失

## 依据

新 VM 启动日志明确显示：Desktop.qml 中的 DesktopEditMode 无法加载，QtQuick.Layouts 和 QtQuick.Effects 报 module is not installed，随后 plasmashell 访问冲突。总线和辅助进程已经启动，不能再将其描述为脚本未执行或仅有通用崩溃码。

此前累计包依赖原始完整 ZIP 提供 Qt 的 QML 模块，清单并未显式覆盖上述模块及基础库。完整 SDK 和原始 ZIP 合并目录中存在这些文件，不代表用户当前目录也存在。PE 导入检查通过也不能证明 qmldir、QML 脚本和动态模块可发现。本轮修复这一覆盖假设。

## 修复

- 显式覆盖桌面使用的 QtQml、QtQuick 根模块及 Layouts/Effects/Window/Controls/Templates/Dialogs/Shapes/LocalStorage/Timeline、Qt5Compat 和 Qt/labs 文件，包含 qmldir、插件与类型元数据。未打包 PDF、3D 等无关大型 Qt 子系统。
- 递归包含这些插件对应的 SDK DLL 依赖，不再因为某 DLL 出现在原始 ZIP 中就省略。携带匹配的 Windows/offscreen 平台插件及 SQLite 驱动。
- 新增 tools/ensure-qml-runtime.py 和清单契约；打包前强制检查，遗漏所需模块文件时明确失败，先于版本写入和 ZIP 创建。
- 扩展真正的 Shell 启动自测：编译 Desktop.qml、DesktopEditMode.qml 和 Panel.qml；原来只检查 Panel.qml 覆盖不足。QML 导入探针现在实际创建 RowLayout 和 MultiEffect。
- 桌面 QML 不可用时，空桌面列表、活动管理器、组件浏览器和更换 containment 后的焦点恢复不再直接解引用缺失根对象。正常加载时的功能保留。

保留所有历史修复，不删除用户配置，不清空主题/布局或 QML 缓存，不修改登录/Shell 安装状态。

## 验证

在工作区内的独立合并运行库中暂时移走 Layouts/Effects 模块目录，以新的最小 QML 文件测试。得到与 VM 日志相同的两个 module is not installed 错误，组件编译退出 1。恢复这两个目录后，同一组件编译退出 0；测试目录均已恢复，未移动 SDK 或用户 VM 的文件。

修复后的合并运行库已通过真实 Shell 自测，桌面、编辑层和面板三个 QML 文件均编译成功；显式 Layouts/Effects 对象导入与创建通过。x64 plasmashell 和 qml-import-probe 已重新编译。编译保留了原有 QStringLiteral 宏警告，不称为零警告。

这证明日志中的模块不可发现条件可以复现并通过完整模块交付修复；不等于已经完成 Windows VM 的登录、绘制、输入、多显示器等交互验收。kglobalaccel、插件元数据及会话后端警告不属于本轮日志中导致 Desktop.qml 加载失败的两个导入错误。

## 交付

20261111-hotfix5-fix25 是全量累计覆盖包，继续覆盖同一个运行库目录。关闭使用旧 DLL 的 KDE 组件后覆盖；不要把单独模块随意放到其他 Qt 程序目录。真实 VM 交互仍待验收，未再次要求用户执行手动测试。

最终验证：实际合并运行库 70 项完整回归通过，646 个 PE 模块导入无缺失。严格重放 KWindowSystem 3 个、Workspace 25 个补丁，10 个源码/头文件与构建树一致。QML 覆盖检查涉及 1,693 项模块文件/基础 DLL（含平台及 SQLite 插件）。验证记录见 fix25-hotfix5-validation.json。
