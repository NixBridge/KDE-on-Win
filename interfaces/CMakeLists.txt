cmake_minimum_required(VERSION 3.21)
project(PlasmaWindowsInterfaces LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../tools/build/msvc-deps.cmake")
find_package(Qt6 REQUIRED COMPONENTS DBus)
include(CMakePackageConfigHelpers)
set(KDE_INSTALL_DBUSINTERFACEDIR bin/data/dbus-1/interfaces)
set(CMAKE_INSTALL_FULL_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
qt_generate_dbus_interface("${KWIN_SOURCE}/src/virtualkeyboard_dbus.h" org.kde.kwin.VirtualKeyboard.xml OPTIONS -A)
qt_generate_dbus_interface("${KWIN_SOURCE}/src/tabletmodemanager.h" org.kde.KWin.TabletModeManager.xml OPTIONS -A)
add_custom_target(interfaces ALL DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.VirtualKeyboard.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/org.kde.KWin.TabletModeManager.xml")
install(FILES
    "${KWIN_SOURCE}/src/org.kde.KWin.xml"
    "${KWIN_SOURCE}/src/org.kde.kwin.Compositing.xml"
    "${KWIN_SOURCE}/src/org.kde.kwin.Effects.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.VirtualKeyboard.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/org.kde.KWin.TabletModeManager.xml"
    DESTINATION "${KDE_INSTALL_DBUSINTERFACEDIR}")
install(FILES "${LOCKER_SOURCE}/dbus/org.freedesktop.ScreenSaver.xml"
    DESTINATION "${KDE_INSTALL_DBUSINTERFACEDIR}" RENAME kf6_org.freedesktop.ScreenSaver.xml)
foreach(name StatusNotifierItem StatusNotifierWatcher)
    install(FILES "${STATUS_NOTIFIER_SOURCE}/src/org.kde.${name}.xml"
        DESTINATION "${KDE_INSTALL_DBUSINTERFACEDIR}" RENAME "kf6_org.kde.${name}.xml")
endforeach()
foreach(name KWinDBusInterface ScreenSaverDBusInterface)
    if(name STREQUAL KWinDBusInterface)
        set(source "${KWIN_SOURCE}")
    else()
        set(source "${LOCKER_SOURCE}")
    endif()
    configure_package_config_file("${source}/${name}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${name}Config.cmake"
        INSTALL_DESTINATION "lib/cmake/${name}"
        PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${name}Config.cmake" DESTINATION "lib/cmake/${name}")
endforeach()
