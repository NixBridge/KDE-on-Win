# Windows launcher and native Plasma applets.
install(FILES "${PROJECT_SOURCE_DIR}/default-panel.js"
    DESTINATION bin/data/plasma/layout-templates/org.kde.plasma.desktop.defaultPanel/contents
    RENAME layout.js)
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick Network)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(Plasma REQUIRED)
find_package(PlasmaQuick REQUIRED)
find_package(LibKWorkspace REQUIRED)
add_library(startmenuplugin MODULE
    "${SOURCE_ROOT}/startmenu/plugin.cpp"
    "${SOURCE_ROOT}/startmenu/windowsiconmap.cpp"
    "${SOURCE_ROOT}/startmenu/windowsshortcut.cpp"
    "${SOURCE_ROOT}/startmenu/startmenumodel.cpp"
    "${SOURCE_ROOT}/startmenu/startmenumodel.h"
    "${SOURCE_ROOT}/startmenu/startmenuimageprovider.cpp"
    "${SOURCE_ROOT}/startmenu/poweractions.cpp"
    "${SOURCE_ROOT}/startmenu/poweractions.h")
set_target_properties(startmenuplugin PROPERTIES PREFIX "")
target_compile_definitions(startmenuplugin PRIVATE UNICODE _UNICODE NOMINMAX)
target_compile_options(startmenuplugin PRIVATE /utf-8 /Zc:__cplusplus)
target_link_libraries(startmenuplugin PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick KF6::IconThemes PW::KWorkspace ole32 user32 shell32 gdi32)
install(TARGETS startmenuplugin RUNTIME DESTINATION qml/org/kde/plasma/windowsmenu LIBRARY DESTINATION qml/org/kde/plasma/windowsmenu)
install(FILES "${SOURCE_ROOT}/startmenu/qmldir" DESTINATION qml/org/kde/plasma/windowsmenu)
foreach(name volumewin imewin windowsmenu)
    set(sources "${SOURCE_ROOT}/applets/${name}/${name}.cpp")
    if(name STREQUAL volumewin)
        list(APPEND sources "${SOURCE_ROOT}/applets/${name}/volumecontroller.cpp" "${SOURCE_ROOT}/applets/${name}/volumecontroller.h")
    elseif(name STREQUAL imewin)
        list(APPEND sources "${SOURCE_ROOT}/applets/${name}/imecontroller.cpp" "${SOURCE_ROOT}/applets/${name}/imecontroller.h")
    endif()
    add_library(${name} MODULE ${sources})
    set_target_properties(${name} PROPERTIES PREFIX "" OUTPUT_NAME "org.kde.plasma.${name}")
    target_compile_definitions(${name} PRIVATE UNICODE _UNICODE NOMINMAX)
    target_compile_options(${name} PRIVATE /utf-8 /Zc:__cplusplus)
    target_link_libraries(${name} PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Plasma::Plasma Plasma::PlasmaQuick KF6::CoreAddons KF6::I18n ole32 uuid user32)
    install(TARGETS ${name} RUNTIME DESTINATION plugins/plasma/applets LIBRARY DESTINATION plugins/plasma/applets)
    install(DIRECTORY "${SOURCE_ROOT}/applets/${name}/plasmoid/" DESTINATION "bin/data/plasma/plasmoids/org.kde.plasma.${name}")
endforeach()
