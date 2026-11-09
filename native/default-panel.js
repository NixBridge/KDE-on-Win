// Windows session layout. All platform applets are installed by this project.
var panel = new Panel;
panel.opacity = "opaque";
panel.height = 2 * Math.ceil(gridUnit * 2.5 / 2);
panel.addWidget("org.kde.plasma.windowsmenu");
var tasks = panel.addWidget("org.kde.plasma.icontasks");
tasks.currentConfigGroup = ["General"];
tasks.writeConfig("launchers", ["applications:org.kde.konsole.desktop", "applications:org.kde.dolphin.desktop", "applications:org.nixbridge.processmonitor.desktop", "applications:org.nixbridge.texteditor.desktop", "applications:org.nixbridge.systemsettings.desktop"]);
panel.addWidget("org.kde.plasma.marginsseparator");
panel.addWidget("org.kde.plasma.systemtray");
panel.addWidget("org.kde.plasma.volumewin");
panel.addWidget("org.kde.plasma.imewin");
panel.addWidget("org.kde.plasma.digitalclock");
// KWin's Peek Desktop is not supported by the native Windows backend.
