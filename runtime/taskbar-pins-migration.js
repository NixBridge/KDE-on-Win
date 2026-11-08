// SPDX-License-Identifier: GPL-3.0-or-later
// Remove only unavailable Linux defaults; preserve users' other pinned items.
function repairUnavailableTaskbarPins(panelList) {
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
            if (filtered.length !== launchers.length) {
                widget.writeConfig("launchers", filtered);
                ++changed;
            }
            widget.currentConfigGroup = previousGroup;
        }
    }
    return { changed: changed, taskManagers: taskManagers };
}
var migration = repairUnavailableTaskbarPins(panels());
print((migration.taskManagers > 0 ? "TASKBAR_PINS_REPAIRED=" : "WAITING_TASKBAR=") + migration.changed);
