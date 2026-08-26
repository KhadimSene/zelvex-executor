#pragma once

#include <QString>
#include <QColor>
#include <QList>
#include <QVariant>

struct CheatEntry {
    int id = 0;
    QString description;
    QString variableType;
    QString address;
    QList<qint64> offsets;
    QColor color;
    QString assemblerScript;
    bool isGroupHeader = false;
    bool activated = false;
    bool hideChildren = false;
    bool alwaysHideChildren = false;
    bool activateChildrenAsWell = false;
    bool deactivateChildrenAsWell = false;
    QString lastStateValue;
    QString lastStateRealAddress;
    bool showAsSigned = false;
    bool dropdownReadOnly = false;
    bool dropdownDescriptionOnly = false;
    bool dropdownDisplayValueAsItem = false;
    QStringList dropDownItems;
    QList<CheatEntry*> children;
    CheatEntry* parent = nullptr;

    ~CheatEntry() {
        qDeleteAll(children);
    }

    bool isScript() const { return variableType == "Auto Assembler Script"; }
    bool isPointer() const { return !address.isEmpty() && !offsets.isEmpty(); }
    bool isMemoryEntry() const { return !address.isEmpty() && variableType != "Auto Assembler Script"; }

    QColor resolvedColor() const {
        if (color.isValid()) return color;
        if (parent) return parent->resolvedColor();
        return QColor(200, 200, 200);
    }

    int depth() const {
        int d = 0;
        const CheatEntry* p = parent;
        while (p) { d++; p = p->parent; }
        return d;
    }
};
