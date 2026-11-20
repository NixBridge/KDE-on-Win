# Fix25 Hotfix7：补齐 Qt Quick Windows Controls 运行库

Hotfix6 覆盖了 QtQuick.Controls.Windows，但遗漏了它从嵌入式 QML 导入的 QtQuick.NativeStyle。显式使用 Windows Controls 样式并移走 NativeStyle 时，重现了重命名框、启动菜单和组件错误窗口的同一导入失败。

全量累计包加入 NativeStyle 模块目录、插件、控件和依赖校验。回归测试显式使用 Windows Controls 编译重命名框、桌面文件夹视图、启动菜单和组件错误窗口，并验证累计清单漏掉 NativeStyle 时打包检查会失败。

KWin 屏幕查询失败后，现有代码会使用当前窗口屏幕或主屏幕。全局快捷键和会话后端警告仍需单独排查。实际 Windows 11 VM 桌面交互仍待验证。
