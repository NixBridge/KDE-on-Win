// Windows session layout. All platform applets are installed by this project.
var panel = new Panel;
panel.opacity = "opaque";
panel.height = 2 * Math.ceil(gridUnit * 2.5 / 2);
panel.addWidget("org.kde.plasma.windowsmenu");
panel.addWidget("org.kde.plasma.icontasks");
panel.addWidget("org.kde.plasma.marginsseparator");
panel.addWidget("org.kde.plasma.systemtray");
panel.addWidget("org.kde.plasma.volumewin");
panel.addWidget("org.kde.plasma.imewin");
panel.addWidget("org.kde.plasma.digitalclock");
panel.addWidget("org.kde.plasma.showdesktop");
