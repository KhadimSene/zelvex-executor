#pragma once

#include <QString>
#include <QByteArray>
#include <QList>
#include <QStringList>

enum class CheatType {
    TOGGLE_AOB_DB,
    TOGGLE_AOB_JMP,
    TOGGLE_THREAD,
    TOGGLE_RANDOM_TP,
    INPUT_FLOAT,
    INPUT_INT,
    COMBO_INT,
    ACTION_WIN32,
    ACTION_AOB_DB,
    ACTION_AOB_JMP,
    ACTION_GIVE_ITEM,
};

struct CheatDef {
    int id = 0;
    const char* name = nullptr;
    const char* tab = nullptr;
    const char* subtitle = nullptr;
    CheatType type = CheatType::TOGGLE_AOB_DB;
    const char* module = nullptr;
    const char* aob = nullptr;
    int patchOffset = 0;
    const char* enableBytes = nullptr;
    int nopCount = 0;
    const char* newmemAsm = nullptr;
    const char* globalAllocName = nullptr;
    int globalAllocSize = 0;
    int win32Type = 0;
    const char* address = nullptr;
    const char* offsetStr = nullptr;
    const char* valueType = nullptr;
    const char* comboItems = nullptr;
    const char* threadAsm = nullptr;
    const char* roleIdAddress = nullptr;
    const char* symbolName = nullptr;
    int symbolOffset = 0;
    int roleIdOffset1 = 0x5754;
    int roleIdOffset2 = 8;
};

inline QList<qint64> parseCheatOffsets(const char* str) {
    QList<qint64> offsets;
    if (!str || !*str) return offsets;
    QStringList parts = QString(str).split(',', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        QStringList hexParts = p.trimmed().split('+', Qt::SkipEmptyParts);
        qint64 val = 0;
        for (const QString& h : hexParts)
            val += h.trimmed().toULongLong(nullptr, 16);
        offsets.append(val);
    }
    return offsets;
}

inline QList<CheatDef> buildCheatDefs() {
    QList<CheatDef> cheats;

    // ═══════════════════════════════════════
    // GENERAL
    // ═══════════════════════════════════════
    cheats.append({1807641558, "README", "general", "Show credits / readme",
        CheatType::ACTION_WIN32});
    cheats.append({999001, "Terrain Editor", "general", "Enable terrain editor + mode 1",
        CheatType::TOGGLE_THREAD, "libSandboxEngine.dll",
        "8B 81 88 00 00 00", 0, nullptr, 0,
        nullptr, nullptr, 0,
        0, nullptr, nullptr, nullptr,
        "cmp dword ptr [libSandboxEngine.g_pPlayerCtrl],#0\nje end\ncmp dword ptr [libSandboxEngine.g_WorldMgr],#0\nje end\npushad\nmov ebx,[libSandboxEngine.g_pPlayerCtrl]\nmov ebx,[ebx+0x56C]\nmov ebx,[ebx+0x240]\nmov ebx,[ebx+0x8]\nmov ebx,[ebx+0x50]\nmov ebx,[ebx+0xAC]\ncmp ebx,#0\nje nhay\ncmp dword ptr [ebx+0x0],#10500\nje endpopad\npush XY\npush #1\npush #1000\npush #12239\nmov ecx,[libSandboxEngine.g_pPlayerCtrl]\nmov eax,libSandboxEngine.MpPlayerControl::setItem\ncall eax\npush #200\npush #1000\nmov ecx,[libSandboxEngine.g_pPlayerCtrl]\nmov eax,libSandboxEngine.MpPlayerControl::discardItem\ncall eax\npush #100\ncall kernel32.sleep\nnhay:\nmov ebx,[libSandboxEngine.dll+247605C]\ncmp dword ptr [ebx+0xBC],#1\nje onroom\npush XY\npush #1\npush #1000\npush #10500\nmov ecx,[libSandboxEngine.g_pPlayerCtrl]\nmov eax,libSandboxEngine.MpPlayerControl::setItem\ncall eax\nonroom:\npush string\npush string1\ncall libSandboxEngine.SandBoxManager::sendToHost\nendpopad:\npopad\nend:\npush #300\ncall kernel32.sleep\njmp XY",
        "libiworld.g_nHomeGardenSaveVersion"});

    // ═══════════════════════════════════════
    // PLAYER
    // ═══════════════════════════════════════
    cheats.append({19, "Noclip", "player", "Walk through walls and obstacles",
        CheatType::TOGGLE_AOB_DB, "libSandboxEngine.dll",
        "74 56 33 C9 8B 13", 0, "90 90 33 C9 8B 13"});
    cheats.append({6, "Condition", "player", "Player movement mode",
        CheatType::COMBO_INT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "23c", "4 Bytes",
        "0:Normal,8:Fly,72:Sprint + Fly"});
    cheats.append({1337198110, "No Fall Damage", "player", "Prevents all fall damage",
        CheatType::TOGGLE_AOB_DB, "libSandboxEngine.dll",
        "0F 84 ?? ?? ?? ?? 8A ?? ?? ?? 84 ?? 0F 85 ?? ?? ?? ?? C6 ?? ?? ?? ?? ?? ?? E9 ?? ?? ?? ?? 80 ?? ?? ?? ??",
        1, "85"});
    cheats.append({1337197986, "Fast Eat", "player", "Instantly consume items and fruits",
        CheatType::TOGGLE_AOB_JMP, "libSandboxEngine.dll",
        "6B ?? ?? ?? 8B ?? ?? 89 ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B ?? ?? 8B ?? ?? ?? ?? ??",
        0, nullptr, 2,
        "imul eax,[eax+0x4],0\nmov ecx,[edi+0x40]\njmp return"});
    // ═══════════════════════════════════════
    // TELEPORTS
    // ═══════════════════════════════════════
    cheats.append({17, "Position X", "teleports", "X coordinate",
        CheatType::INPUT_INT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "e8,270", "4 Bytes"});
    cheats.append({15, "Position Y", "teleports", "Y coordinate",
        CheatType::INPUT_INT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "e0,270", "4 Bytes"});
    cheats.append({16, "Position Z", "teleports", "Z coordinate",
        CheatType::INPUT_INT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "e4,270", "4 Bytes"});
    cheats.append({999010, "Random Teleport", "teleports", "Teleport to random location",
        CheatType::TOGGLE_RANDOM_TP, "libSandboxEngine.dll",
        "8B 81 88 00 00 00", 0, nullptr, 0,
        nullptr, nullptr, 0,
        0, nullptr, nullptr, nullptr,
        "", "libiworld.g_nHomeGardenSaveVersion"});

    // ═══════════════════════════════════════
    // STATS
    // ═══════════════════════════════════════
    cheats.append({22, "Health", "stats", "Current health value",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "94,280", "Float"});
    cheats.append({28, "MaxHealth", "stats", "Maximum health value",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "98,280", "Float"});
    cheats.append({27, "Hunger", "stats", "Current hunger value",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "220,280", "Float"});
    cheats.append({29, "MaxHunger", "stats", "Maximum hunger value",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "22c,280", "Float"});
    cheats.append({30, "Stamina", "stats", "Current stamina value",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "280,280", "Float"});
    cheats.append({33, "MaxStamina", "stats", "Maximum stamina value",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "288,280", "Float"});

    // ═══════════════════════════════════════
    // MOVEMENT
    // ═══════════════════════════════════════
    cheats.append({1337198106, "Air Jump", "movement", "Unlimited mid-air jumps",
        CheatType::TOGGLE_AOB_JMP, "libSandboxEngine.dll",
        "80 B8 A4 00 00 00 00 74 5A", 0, nullptr,
        2, "mov byte ptr [eax+0xA4],0x1\ncmp byte ptr [eax+0xA4],0x00\njmp return"});
    cheats.append({1807641586, "Swim in Air", "movement", "Swim while airborne",
        CheatType::TOGGLE_AOB_JMP, "libSandboxEngine.dll",
        "66 C7 80 64 01 00 00 00 00", 0, nullptr,
        4, "mov word ptr [eax+0x164],0x0001\njmp return"});
    cheats.append({23, "WalkSpeed", "movement", "Walking speed multiplier",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "cc,280", "Float"});
    cheats.append({24, "RunSpeed", "movement", "Running speed multiplier",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "d0,280", "Float"});
    cheats.append({25, "CrouchSpeed", "movement", "Crouching speed multiplier",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "d4,280", "Float"});
    cheats.append({26, "SwimSpeed", "movement", "Swimming speed multiplier",
        CheatType::INPUT_FLOAT, nullptr, nullptr, 0, nullptr, 0,         nullptr, nullptr, 0,
        0, "libSandboxEngine.dll+27772F8", "d8,280", "Float"});

    // ═══════════════════════════════════════
    // COMBAT
    // ═══════════════════════════════════════
    cheats.append({1337197721, "Auto Click Left", "combat", "Auto left-click",
        CheatType::TOGGLE_AOB_DB, "libSandboxEngine.dll",
        "74 ?? F3 0F 10 45 ?? 8D 45 ?? 50 51 8B CE C6 45", 0, "90"});
    cheats.append({1337197722, "Auto Click Right", "combat", "Auto right-click",
        CheatType::TOGGLE_AOB_DB, "libSandboxEngine.dll",
        "74 ?? F3 0F 10 45 ?? 8D 45 ?? 50 51 8B CE F3", 0, "70"});
    cheats.append({1337212286, "Force Spectator", "combat", "Force spectator mode on target",
        CheatType::TOGGLE_AOB_JMP, "libSandboxEngine.dll",
        "8B 52 38 FF D2 8B 4F 40 8B 77", 0, nullptr, 0,
        "pushad\nmov ecx,[libSandboxEngine.g_pPlayerCtrl]\nmov ecx,[ecx+0x4D0]\npush #2\ncall libSandboxEngine.PlayerControl::setSpectatorMode\npopad\nmov edx,[edx+0x38]\ncall edx\njmp return"});
    cheats.append({1807635788, "Disable Spectator", "combat", "Disable spectator mode on target",
        CheatType::ACTION_AOB_JMP, "libSandboxEngine.dll",
        "8B 04 81 5D C2 04 00 8D 81", 0, nullptr, 2,
        "mov eax,[ecx+eax*4]\npush ecx\npush eax\npush #0\nmov ecx,eax\ncall libSandboxEngine.PlayerControl::setSpectatorMode\npop eax\npop ecx\npop ebp\nret 0004\njmp return"});
    cheats.append({1807617779, "Crash Host", "combat", "Spam item 1105 to crash the host",
        CheatType::TOGGLE_THREAD, "libSandboxEngine.dll",
        nullptr, 0, nullptr, 0,
        nullptr, nullptr, 0,
        0, nullptr, nullptr, nullptr,
        "XY:\npushad\npush string\npush string1\ncall libSandboxEngine.SandBoxManager::sendToHost\npopad\npush #1\ncall kernel32.sleep\njmp XY",
        "libSandboxEngine.g_WorldMgr", nullptr, 0, 0x8C, 0});

    // ═══════════════════════════════════════
    // VISION
    // ═══════════════════════════════════════
    cheats.append({1807652946, "Show Hitboxes", "vision", "Show player hitboxes",
        CheatType::TOGGLE_AOB_DB, "libSandboxEngine.dll",
        nullptr, 0, "75 0A", 0,
        nullptr, nullptr, 0,
        0, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        "ClientActor::update", 0x1B2});
    cheats.append({1807641585, "Night Vision", "vision", "Brighten the sky for better visibility",
        CheatType::TOGGLE_AOB_JMP, "libEngine.dll",
        "F3 0F 10 08 F3 0F 10 50 04 F3 0F 10 58 08 8B 45 08", 0, nullptr,
        4, "mov [eax],(float)0.7039216161\nmov [eax+4],(float)0.6901960969\nmov [eax+8],(float)0.8117647171\nmovss xmm1,[eax]\nmovss xmm2,[eax+04]\njmp return"});

    // ═══════════════════════════════════════
    // ITEMS
    // ═══════════════════════════════════════
    cheats.append({999100, "Give Item", "items", "Send item to host (DEVELOPERSTORE)",
        CheatType::ACTION_GIVE_ITEM, "libSandboxEngine.dll"});

    return cheats;
}
