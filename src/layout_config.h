#pragma once

#include <QList>
#include <QString>

#include "app_icons.h"

namespace UILayout {

struct TabDef {
    QString label;
    QString letter;
    bool logo = false;
    AppIcons::Id icon;
    QList<int> cheatIds;
    QList<QString> sectionHeaders;
};

inline QList<TabDef> getTabDefs() {
    return {
        {"Misc", "M", false, AppIcons::Id::Misc, {}, {"GENERAL", "Zelvex Native World"}},
        {"Player", "P", false, AppIcons::Id::Player, {19, 6, 1337198110, 1337197986}, {"{Player Cheats}", "Zelvex Native Player"}},
        {"Teleport", "T", false, AppIcons::Id::Teleport, {17, 15, 16, 999010}, {"Position", "Teleport Config"}},
        {"Stats", "S", false, AppIcons::Id::Stats, {22, 28, 27, 29, 30, 33}, {"Health/Hunger/Stamina Stats"}},
        {"Movement", "W", false, AppIcons::Id::Movement, {1337198106, 1807641586, 23, 24, 25, 26}, {"Movement Cheats", "Speed Stats"}},
        {"Combat", "C", false, AppIcons::Id::Combat, {1337197721, 1337197722, 1337212286, 1807635788, 1807617779}, {"Auto Click", "Spectator", "Offensive"}},
        {"Vision", "V", false, AppIcons::Id::Vision, {1807652946, 1807641585}, {"Vision Cheats", "Zelvex Native Vision"}},
        {"Items", "I", false, AppIcons::Id::Items, {999100}, {"Item Management", "Zelvex Native Items"}},
        {"Lua", "L", false, AppIcons::Id::Lua, {}, {"Lua Code Injector"}},
    };
}

} // namespace UILayout