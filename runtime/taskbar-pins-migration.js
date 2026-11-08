// SPDX-License-Identifier: GPL-3.0-or-later
// Remove only unavailable Linux defaults; preserve users' other pinned items.
function repairUnavailableTaskbarPins(panelList, desktopToolsAvailable, desktopExtrasAvailable) {
    var changed = 0;
    var taskManagers = 0;
    for (var p = 0; p < panelList.length; ++p) {
        var widgets = panelList[p].widgets();
        for (var i = 0; i < widgets.length; ++i) {
            var widget = widgets[i];
            if (widget.type !== "org.kde.plasma.icontasks" && widget.type !== "org.kde.plasma.taskmanager") continue;
            ++taskManagers;
            var previousGroup = widget.currentConfigGroup;
            widget.currentConfigGroup = ["General"];
            // readConfig reads the stored group, not the widget schema default.
            var launchers = widget.readConfig("launchers", ["applications:systemsettings.desktop", "applications:org.kde.discover.desktop", "preferred://filemanager", "preferred://browser"]);
            if (typeof launchers === "string") launchers = launchers ? launchers.split(",") : [];
            var filtered = [];
            for (var n = 0; n < launchers.length; ++n) {
                if (!/^(applications:)?(org\.kde\.discover\.desktop|systemsettings\.desktop)$/.test(launchers[n])) filtered.push(launchers[n]);
            }
            var addTools = desktopToolsAvailable && Number(widget.readConfig("windowsDesktopToolsVersion", 0)) < 1;
            if (addTools) {
                var tools = ["applications:org.kde.konsole.desktop", "applications:org.nixbridge.processmonitor.desktop"];
                for (var t = 0; t < tools.length; ++t) if (filtered.indexOf(tools[t]) < 0) filtered.push(tools[t]);
            }
            var addExtras = desktopExtrasAvailable && Number(widget.readConfig("windowsDesktopExtrasVersion", 0)) < 1;
            if (addExtras) {
                var extras = ["applications:org.nixbridge.texteditor.desktop", "applications:org.nixbridge.systemsettings.desktop"];
                for (var x = 0; x < extras.length; ++x) if (filtered.indexOf(extras[x]) < 0) filtered.push(extras[x]);
            }
            if (filtered.join(",") !== Array.prototype.join.call(launchers, ",")) {
                widget.writeConfig("launchers", filtered);
                ++changed;
            }
            if (addTools) widget.writeConfig("windowsDesktopToolsVersion", 1);
            if (addExtras) widget.writeConfig("windowsDesktopExtrasVersion", 1);
            widget.currentConfigGroup = previousGroup;
        }
    }
    return { changed: changed, taskManagers: taskManagers };
}
var migration = repairUnavailableTaskbarPins(panels(), typeof kdeDesktopToolsAvailable !== "undefined" && kdeDesktopToolsAvailable, typeof kdeDesktopExtrasAvailable !== "undefined" && kdeDesktopExtrasAvailable);
print((migration.taskManagers > 0 ? "TASKBAR_PINS_REPAIRED=" : "WAITING_TASKBAR=") + migration.changed);
