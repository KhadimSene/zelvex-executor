#include "ct_parser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QRegularExpression>

CtParser::~CtParser() {
    if (m_root) delete m_root;
}

bool CtParser::parse(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    return parseData(file.readAll());
}

bool CtParser::parseData(const QByteArray& xmlData) {
    if (m_root) { delete m_root; m_root = nullptr; }
    m_flatEntries.clear();

    QXmlStreamReader xml(xmlData);
    m_root = new CheatEntry();
    m_root->description = "Root";

    CheatEntry* currentParent = m_root;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"CheatTable") {
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == u"CheatTable") break;
                if (xml.isStartElement() && xml.name() == u"CheatEntries") {
                    while (!xml.atEnd()) {
                        xml.readNext();
                        if (xml.isEndElement() && xml.name() == u"CheatEntries") break;
                        if (xml.isStartElement() && xml.name() == u"CheatEntry") {
                            CheatEntry* entry = new CheatEntry();
                            entry->parent = currentParent;
                            currentParent->children.append(entry);

                            // Parse this entry's inner elements
                            while (!xml.atEnd()) {
                                xml.readNext();
                                if (xml.isEndElement() && xml.name() == u"CheatEntry") break;
                                if (!xml.isStartElement()) continue;

                                if (xml.name() == u"ID") {
                                    entry->id = xml.readElementText().toInt();
                                } else if (xml.name() == u"Description") {
                                    QString desc = xml.readElementText();
                                    if (desc.startsWith('"') && desc.endsWith('"'))
                                        desc = desc.mid(1, desc.length() - 2);
                                    desc.replace("&lt;", "<");
                                    desc.replace("&gt;", ">");
                                    desc.replace("&amp;", "&");
                                    entry->description = desc;
                                } else if (xml.name() == u"VariableType") {
                                    entry->variableType = xml.readElementText();
                                } else if (xml.name() == u"Address") {
                                    entry->address = xml.readElementText();
                                } else if (xml.name() == u"Color") {
                                    QString c = xml.readElementText();
                                    if (c.length() == 6) {
                                        bool ok;
                                        quint32 rgb = c.toUInt(&ok, 16);
                                        if (ok)
                                            entry->color = QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                                    }
                                } else if (xml.name() == u"AssemblerScript") {
                                    entry->assemblerScript = xml.readElementText();
                                } else if (xml.name() == u"GroupHeader") {
                                    entry->isGroupHeader = (xml.readElementText() == "1");
                                } else if (xml.name() == u"ShowAsSigned") {
                                    entry->showAsSigned = (xml.readElementText() == "1");
                                } else if (xml.name() == u"Options") {
                                    QXmlStreamAttributes attrs = xml.attributes();
                                    entry->hideChildren = attrs.value("moHideChildren") == "1";
                                    entry->alwaysHideChildren = attrs.value("moAlwaysHideChildren") == "1";
                                    entry->activateChildrenAsWell = attrs.value("moActivateChildrenAsWell") == "1";
                                    entry->deactivateChildrenAsWell = attrs.value("moDeactivateChildrenAsWell") == "1";
                                    if (xml.isStartElement()) {
                                        // Options is self-closing, read until end
                                        while (!xml.atEnd() && !xml.isEndElement())
                                            xml.readNext();
                                    }
                                } else if (xml.name() == u"LastState") {
                                    QXmlStreamAttributes attrs = xml.attributes();
                                    entry->lastStateValue = attrs.value("Value").toString();
                                    entry->lastStateRealAddress = attrs.value("RealAddress").toString();
                                    entry->activated = (attrs.value("Activated") == "1");
                                    if (xml.isStartElement()) {
                                        while (!xml.atEnd() && !xml.isEndElement())
                                            xml.readNext();
                                    }
                                } else if (xml.name() == u"Offsets") {
                                    while (!xml.atEnd()) {
                                        xml.readNext();
                                        if (xml.isEndElement() && xml.name() == u"Offsets") break;
                                        if (xml.isStartElement() && xml.name() == u"Offset") {
                                            QString offsetStr = xml.readElementText();
                                            entry->offsets.append(parseOffsetExpression(offsetStr));
                                        }
                                    }
                                } else if (xml.name() == u"DropDownList") {
                                    QXmlStreamAttributes attrs = xml.attributes();
                                    entry->dropdownReadOnly = (attrs.value("ReadOnly") == "1");
                                    entry->dropdownDescriptionOnly = (attrs.value("DescriptionOnly") == "1");
                                    entry->dropdownDisplayValueAsItem = (attrs.value("DisplayValueAsItem") == "1");
                                    QString listText = xml.readElementText();
                                    QStringList lines = listText.split('\n', Qt::SkipEmptyParts);
                                    for (const QString& line : lines) {
                                        QString trimmed = line.trimmed();
                                        if (!trimmed.isEmpty())
                                            entry->dropDownItems.append(trimmed);
                                    }
                                } else if (xml.name() == u"CheatEntries") {
                                    // Nested cheat entries - recurse
                                    while (!xml.atEnd()) {
                                        xml.readNext();
                                        if (xml.isEndElement() && xml.name() == u"CheatEntries") break;
                                        if (xml.isStartElement() && xml.name() == u"CheatEntry") {
                                            CheatEntry* child = new CheatEntry();
                                            child->parent = entry;
                                            entry->children.append(child);

                                            while (!xml.atEnd()) {
                                                xml.readNext();
                                                if (xml.isEndElement() && xml.name() == u"CheatEntry") break;
                                                if (!xml.isStartElement()) continue;

                                                if (xml.name() == u"ID") {
                                                    child->id = xml.readElementText().toInt();
                                                } else if (xml.name() == u"Description") {
                                                    QString desc = xml.readElementText();
                                                    if (desc.startsWith('"') && desc.endsWith('"'))
                                                        desc = desc.mid(1, desc.length() - 2);
                                                    desc.replace("&lt;", "<");
                                                    desc.replace("&gt;", ">");
                                                    desc.replace("&amp;", "&");
                                                    child->description = desc;
                                                } else if (xml.name() == u"VariableType") {
                                                    child->variableType = xml.readElementText();
                                                } else if (xml.name() == u"Address") {
                                                    child->address = xml.readElementText();
                                                } else if (xml.name() == u"Color") {
                                                    QString c = xml.readElementText();
                                                    if (c.length() == 6) {
                                                        bool ok;
                                                        quint32 rgb = c.toUInt(&ok, 16);
                                                        if (ok)
                                                            child->color = QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                                                    }
                                                } else if (xml.name() == u"AssemblerScript") {
                                                    child->assemblerScript = xml.readElementText();
                                                } else if (xml.name() == u"GroupHeader") {
                                                    child->isGroupHeader = (xml.readElementText() == "1");
                                                } else if (xml.name() == u"ShowAsSigned") {
                                                    child->showAsSigned = (xml.readElementText() == "1");
                                                } else if (xml.name() == u"Options") {
                                                    QXmlStreamAttributes attrs = xml.attributes();
                                                    child->hideChildren = attrs.value("moHideChildren") == "1";
                                                    child->alwaysHideChildren = attrs.value("moAlwaysHideChildren") == "1";
                                                    child->activateChildrenAsWell = attrs.value("moActivateChildrenAsWell") == "1";
                                                    child->deactivateChildrenAsWell = attrs.value("moDeactivateChildrenAsWell") == "1";
                                                    if (xml.isStartElement()) {
                                                        while (!xml.atEnd() && !xml.isEndElement())
                                                            xml.readNext();
                                                    }
                                                } else if (xml.name() == u"LastState") {
                                                    QXmlStreamAttributes attrs = xml.attributes();
                                                    child->lastStateValue = attrs.value("Value").toString();
                                                    child->lastStateRealAddress = attrs.value("RealAddress").toString();
                                                    child->activated = (attrs.value("Activated") == "1");
                                                    if (xml.isStartElement()) {
                                                        while (!xml.atEnd() && !xml.isEndElement())
                                                            xml.readNext();
                                                    }
                                                } else if (xml.name() == u"Offsets") {
                                                    while (!xml.atEnd()) {
                                                        xml.readNext();
                                                        if (xml.isEndElement() && xml.name() == u"Offsets") break;
                                                        if (xml.isStartElement() && xml.name() == u"Offset") {
                                                            QString offsetStr = xml.readElementText();
                                                            child->offsets.append(parseOffsetExpression(offsetStr));
                                                        }
                                                    }
                                                } else if (xml.name() == u"DropDownList") {
                                                    QXmlStreamAttributes attrs = xml.attributes();
                                                    child->dropdownReadOnly = (attrs.value("ReadOnly") == "1");
                                                    child->dropdownDescriptionOnly = (attrs.value("DescriptionOnly") == "1");
                                                    child->dropdownDisplayValueAsItem = (attrs.value("DisplayValueAsItem") == "1");
                                                    QString listText = xml.readElementText();
                                                    QStringList lines = listText.split('\n', Qt::SkipEmptyParts);
                                                    for (const QString& line : lines) {
                                                        QString trimmed = line.trimmed();
                                                        if (!trimmed.isEmpty())
                                                            child->dropDownItems.append(trimmed);
                                                    }
                                                } else if (xml.name() == u"CheatEntries") {
                                                    // Deeper nesting - parse entries into child
                                                    while (!xml.atEnd()) {
                                                        xml.readNext();
                                                        if (xml.isEndElement() && xml.name() == u"CheatEntries") break;
                                                        if (xml.isStartElement() && xml.name() == u"CheatEntry") {
                                                            CheatEntry* grandchild = new CheatEntry();
                                                            grandchild->parent = child;
                                                            child->children.append(grandchild);

                                                            while (!xml.atEnd()) {
                                                                xml.readNext();
                                                                if (xml.isEndElement() && xml.name() == u"CheatEntry") break;
                                                                if (!xml.isStartElement()) continue;

                                                                if (xml.name() == u"ID") {
                                                                    grandchild->id = xml.readElementText().toInt();
                                                                } else if (xml.name() == u"Description") {
                                                                    QString desc = xml.readElementText();
                                                                    if (desc.startsWith('"') && desc.endsWith('"'))
                                                                        desc = desc.mid(1, desc.length() - 2);
                                                                    desc.replace("&lt;", "<");
                                                                    desc.replace("&gt;", ">");
                                                                    desc.replace("&amp;", "&");
                                                                    grandchild->description = desc;
                                                                } else if (xml.name() == u"VariableType") {
                                                                    grandchild->variableType = xml.readElementText();
                                                                } else if (xml.name() == u"Address") {
                                                                    grandchild->address = xml.readElementText();
                                                                } else if (xml.name() == u"Color") {
                                                                    QString c = xml.readElementText();
                                                                    if (c.length() == 6) {
                                                                        bool ok;
                                                                        quint32 rgb = c.toUInt(&ok, 16);
                                                                        if (ok)
                                                                            grandchild->color = QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                                                                    }
                                                                } else if (xml.name() == u"AssemblerScript") {
                                                                    grandchild->assemblerScript = xml.readElementText();
                                                                } else if (xml.name() == u"GroupHeader") {
                                                                    grandchild->isGroupHeader = (xml.readElementText() == "1");
                                                                } else if (xml.name() == u"ShowAsSigned") {
                                                                    grandchild->showAsSigned = (xml.readElementText() == "1");
                                                                } else if (xml.name() == u"Options") {
                                                                    QXmlStreamAttributes attrs = xml.attributes();
                                                                    grandchild->hideChildren = attrs.value("moHideChildren") == "1";
                                                                    grandchild->alwaysHideChildren = attrs.value("moAlwaysHideChildren") == "1";
                                                                    grandchild->activateChildrenAsWell = attrs.value("moActivateChildrenAsWell") == "1";
                                                                    grandchild->deactivateChildrenAsWell = attrs.value("moDeactivateChildrenAsWell") == "1";
                                                                    if (xml.isStartElement()) {
                                                                        while (!xml.atEnd() && !xml.isEndElement())
                                                                            xml.readNext();
                                                                    }
                                                                } else if (xml.name() == u"LastState") {
                                                                    QXmlStreamAttributes attrs = xml.attributes();
                                                                    grandchild->lastStateValue = attrs.value("Value").toString();
                                                                    grandchild->lastStateRealAddress = attrs.value("RealAddress").toString();
                                                                    grandchild->activated = (attrs.value("Activated") == "1");
                                                                    if (xml.isStartElement()) {
                                                                        while (!xml.atEnd() && !xml.isEndElement())
                                                                            xml.readNext();
                                                                    }
                                                                } else if (xml.name() == u"Offsets") {
                                                                    while (!xml.atEnd()) {
                                                                        xml.readNext();
                                                                        if (xml.isEndElement() && xml.name() == u"Offsets") break;
                                                                        if (xml.isStartElement() && xml.name() == u"Offset") {
                                                                            QString offsetStr = xml.readElementText();
                                                                            grandchild->offsets.append(parseOffsetExpression(offsetStr));
                                                                        }
                                                                    }
                                                                } else if (xml.name() == u"DropDownList") {
                                                                    QXmlStreamAttributes attrs = xml.attributes();
                                                                    grandchild->dropdownReadOnly = (attrs.value("ReadOnly") == "1");
                                                                    grandchild->dropdownDescriptionOnly = (attrs.value("DescriptionOnly") == "1");
                                                                    grandchild->dropdownDisplayValueAsItem = (attrs.value("DisplayValueAsItem") == "1");
                                                                    QString listText = xml.readElementText();
                                                                    QStringList lines = listText.split('\n', Qt::SkipEmptyParts);
                                                                    for (const QString& line : lines) {
                                                                        QString trimmed = line.trimmed();
                                                                        if (!trimmed.isEmpty())
                                                                            grandchild->dropDownItems.append(trimmed);
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Flatten tree for quick lookup
    std::function<void(CheatEntry*)> flatten = [&](CheatEntry* e) {
        m_flatEntries.append(e);
        for (CheatEntry* child : e->children)
            flatten(child);
    };
    flatten(m_root);

    return !xml.hasError();
}

int CtParser::parseOffsetExpression(const QString& expr) {
    // Handle expressions like "94+18c", "94+1f4", "94+38", "94+40", "94+44"
    // Simple approach: sum all hex numbers in the expression
    int result = 0;
    QRegularExpression re("([0-9a-fA-F]+)");
    QRegularExpressionMatchIterator it = re.globalMatch(expr);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        bool ok;
        result += match.captured(1).toUInt(&ok, 16);
    }
    return result;
}
