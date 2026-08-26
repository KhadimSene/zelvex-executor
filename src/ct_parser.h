#pragma once

#include <QString>
#include <QList>
#include <QByteArray>
#include "cheat_entry.h"

class CtParser {
public:
    bool parse(const QString& filePath);
    bool parseData(const QByteArray& xmlData);
    CheatEntry* rootEntry() const { return m_root; }
    QList<CheatEntry*> flatEntries() const { return m_flatEntries; }
    ~CtParser();

private:
    int parseOffsetExpression(const QString& expr);
    CheatEntry* m_root = nullptr;
    QList<CheatEntry*> m_flatEntries;
};
