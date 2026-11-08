/*
    SPDX-FileCopyrightText: 2026 Plasma Windows contributors
    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PC3
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami
import org.kde.plasma.windowsmenu

PlasmoidItem {
    id: root

    // Panel: a start button. Clicking expands the full menu.
    compactRepresentation: MouseArea {
        id: compactArea
        Layout.preferredWidth: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
        Layout.preferredHeight: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2

        Kirigami.Icon {
            anchors.centerIn: parent
            source: "start-here-symbolic"
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
        }
        onClicked: root.expanded = !root.expanded
    }

    // The menu itself (shown in the popup when expanded).
    fullRepresentation: Rectangle {
        color: Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 1)
        radius: Kirigami.Units.smallSpacing
        clip: true
        id: menuRoot
        // Explicit size: the popup sizes itself from the content's
        // implicit size - a ColumnLayout-wrapped sidebar collapses the
        // implicit width to ~0 (3px grid observed) without this.
        implicitWidth: Kirigami.Units.gridUnit * 36
        implicitHeight: Kirigami.Units.gridUnit * 28
        Layout.preferredWidth: implicitWidth
        Layout.preferredHeight: implicitHeight

        StartMenuModel {
            id: menu
            directory: "all"
        }

        PowerActions {
            id: power
        }

        // Sidebar: All Apps + user-configured directories.
        property var sidebarDirs: []
        property int contextRow: -1
        Shortcut { sequence: StandardKey.Find; enabled: root.expanded; onActivated: search.forceActiveFocus() }

        function sidebarModel() {
            const cats = []
            for (let i = 0; i < menu.categories.length; i++) {
                cats.push({ name: menu.categories[i].name, path: "cat:" + menu.categories[i].name, icon: menu.categories[i].icon || "folder-symbolic" })
            }
            return [
                { name: i18n("All Apps"), path: "all", icon: "view-grid-symbolic" },
                ...cats,
                ...sidebarDirs,
            ]
        }

        function currentSidebarIndex() {
            const list = sidebarModel()
            for (let i = 0; i < list.length; i++) {
                if ((list[i].path === "all" && menu.isRoot) || (list[i].path !== "all" && menu.directory === list[i].path)) {
                    return i
                }
            }
            return 0
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            // Left column: sidebar (All Apps + categories) and the
            // power menu at the bottom. Plain Column with explicit
            // width: a nested ColumnLayout here made the RowLayout
            // assign the whole width to this column (implicit-size
            // loop through the inner fillWidth items) and squeezed
            // the right column to a few px.
            Column {
                id: leftCol
                Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                Layout.fillHeight: true
                width: Kirigami.Units.gridUnit * 9
                spacing: Kirigami.Units.smallSpacing

                // Left: sidebar (All Apps + user dirs)
                ListView {
                    id: sidebarView
                    width: parent.width
                    height: parent.height - powerButton.height - parent.spacing

                    model: menuRoot.sidebarModel()
                    currentIndex: menuRoot.currentSidebarIndex()
                    clip: true

                    delegate: PC3.ToolButton {
                        id: sideButton
                        width: sidebarView.width
                        text: modelData.name
                        icon.name: modelData.icon || "folder-symbolic"
                        contentItem: RowLayout {
                            Image { source: "image://breezefileicons/" + encodeURIComponent("breeze:theme:" + (modelData.icon || "folder")); Layout.preferredWidth: Kirigami.Units.iconSizes.small; Layout.preferredHeight: Kirigami.Units.iconSizes.small; fillMode: Image.PreserveAspectFit }
                            PC3.Label { text: sideButton.text; textFormat: Text.PlainText; Layout.fillWidth: true; elide: Text.ElideRight }
                        }
                        highlighted: sidebarView.currentIndex === index
                        onClicked: {
                            sidebarView.currentIndex = index
                            menu.directory = modelData.path
                        }
                    }
                }

                // Power menu (shutdown / reboot / suspend / hibernate / lock)
                PC3.ToolButton {
                    id: powerButton
                    width: parent.width
                    text: i18n("Power")
                    icon.name: "system-shutdown-symbolic"
                    onClicked: powerMenu.popup(powerButton, powerButton.width - powerMenu.width, powerButton.height)
                }
            }

            // Right: current view (all apps grid or dir contents)
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Kirigami.Units.smallSpacing
                RowLayout {
                    Layout.fillWidth: true
                    PC3.TextField {
                        id: search
                        Layout.fillWidth: true
                        placeholderText: i18n("Search this folder…")
                        text: menu.searchText
                        onTextEdited: menu.searchText = text
                        Keys.onDownPressed: { grid.forceActiveFocus(); grid.currentIndex = 0 }
                        Keys.onEscapePressed: { if (text.length) menu.searchText = ""; else root.expanded = false }
                    }
                    PC3.ToolButton { icon.name: "view-refresh"; text: i18n("Refresh"); onClicked: menu.reload() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !menu.isRoot
                    PC3.ToolButton {
                        icon.name: "go-previous-symbolic"
                        onClicked: menu.goParent()
                    }
                    Kirigami.Heading {
                        level: 4
                        text: menu.directoryName
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }

                GridView {
                    id: grid
                    PC3.ScrollBar.vertical: PC3.ScrollBar { }
                    focus: true
                    Keys.onReturnPressed: { if (menu.activate(currentIndex)) root.expanded = false }
                    Keys.onEscapePressed: root.expanded = false
                    Keys.onBackPressed: menu.goParent()
                    Keys.onPressed: (event) => { if (event.key === Qt.Key_Backspace) { menu.goParent(); event.accepted = true } }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: menu
                    clip: true
                    cacheBuffer: Kirigami.Units.gridUnit * 8
                    cellWidth: width / menu.columns
                    cellHeight: Math.max(Kirigami.Units.gridUnit * 6, menu.iconPixels + Kirigami.Units.gridUnit * 4)

                delegate: Item {
                    id: delegateItem
                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        radius: Kirigami.Units.smallSpacing
                        color: Kirigami.Theme.highlightColor
                        opacity: itemMouse.containsMouse || (grid.activeFocus && grid.currentIndex === index) ? 0.16 : 0
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing * 2
                        spacing: Kirigami.Units.smallSpacing

                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            width: menu.iconPixels
                            height: menu.iconPixels

                            Image {
                                anchors.fill: parent
                                source: "image://breezefileicons/" + encodeURIComponent("breeze:" + (model.isDir ? "theme:" + (model.icon || "folder") : model.linkPath))
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        PC3.Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: model.name
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignTop
                            color: Kirigami.Theme.textColor
                            font: Kirigami.Theme.defaultFont
                        }
                    }

                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            grid.currentIndex = index
                            grid.forceActiveFocus()
                            if (mouse.button === Qt.RightButton) { menuRoot.contextRow = index; itemMenu.popup(); return }
                            if (menu.activate(index)) root.expanded = false
                        }
                    }
                    PC3.ToolTip.visible: itemMouse.containsMouse
                    PC3.ToolTip.text: model.name
                }
                }
            }
        }

        PC3.Menu {
            id: itemMenu
            PC3.MenuItem { text: i18n("Open"); icon.name: "document-open"; onClicked: { if (menu.activate(menuRoot.contextRow)) root.expanded = false } }
            PC3.MenuItem { text: i18n("Open Location"); icon.name: "folder-open"; onClicked: { menu.reveal(menuRoot.contextRow); root.expanded = false } }
        }
        PC3.Menu {
            id: powerMenu
            title: i18n("Power")

            PC3.MenuItem {
                text: i18n("Shut Down")
                icon.name: "system-shutdown-symbolic"
                enabled: power.canShutdown
                onClicked: {
                    root.expanded = false
                    power.shutdown()
                }
            }
            PC3.MenuItem {
                text: i18n("Restart")
                icon.name: "system-reboot-symbolic"
                enabled: power.canReboot
                onClicked: {
                    root.expanded = false
                    power.reboot()
                }
            }
            PC3.MenuItem {
                text: i18n("Sleep")
                icon.name: "system-suspend-symbolic"
                enabled: power.canSuspend
                onClicked: {
                    root.expanded = false
                    power.suspend()
                }
            }
            PC3.MenuItem {
                text: i18n("Hibernate")
                icon.name: "system-suspend-hibernate-symbolic"
                enabled: power.canHibernate
                onClicked: {
                    root.expanded = false
                    power.hibernate()
                }
            }
            PC3.MenuItem {
                text: i18n("Lock Screen")
                icon.name: "system-lock-screen-symbolic"
                enabled: power.canLock
                onClicked: {
                    root.expanded = false
                    power.lock()
                }
            }
        }
    }
}
