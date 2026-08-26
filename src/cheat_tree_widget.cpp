#include "cheat_tree_widget.h"
#include <QCheckBox>

CheatTreeWidget::CheatTreeWidget(QWidget* parent) : QTreeWidget(parent) {
    setHeaderLabels({"Activate", "Description", "Type", "Value", "Address"});
    setColumnWidth(0, 40);
    setColumnWidth(1, 250);
    setColumnWidth(2, 120);
    setColumnWidth(3, 150);
    setColumnWidth(4, 200);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setRootIsDecorated(true);
    setAnimated(true);
    setExpandsOnDoubleClick(true);

    connect(this, &QTreeWidget::itemChanged, this, &CheatTreeWidget::onItemChanged);
    connect(this, &QTreeWidget::itemSelectionChanged, this, &CheatTreeWidget::onItemSelectionChanged);
}

void CheatTreeWidget::loadEntries(CheatEntry* root) {
    m_updating = true;
    clearEntries();

    if (!root) { m_updating = false; return; }

    for (CheatEntry* child : root->children) {
        QTreeWidgetItem* item = buildTree(child);
        addTopLevelItem(item);
    }

    m_updating = false;
    expandAll();
}

void CheatTreeWidget::clearEntries() {
    m_itemToEntry.clear();
    m_entryToItem.clear();
    m_activated.clear();
    clear();
}

QTreeWidgetItem* CheatTreeWidget::buildTree(CheatEntry* entry) {
    auto* item = new QTreeWidgetItem();
    m_itemToEntry[item] = entry;
    m_entryToItem[entry] = item;

    // Activate checkbox column
    auto* checkWidget = new QCheckBox();
    checkWidget->setChecked(entry->activated);
    connect(checkWidget, &QCheckBox::toggled, this, [this, entry](bool checked) {
        if (m_updating) return;
        m_updating = true;
        if (checked) {
            m_activated.insert(entry);
            entry->activated = true;
            emit cheatActivated(entry);
        } else {
            m_activated.remove(entry);
            entry->activated = false;
            emit cheatDeactivated(entry);
        }
        m_updating = false;
    });
    setItemWidget(item, 0, checkWidget);

    // Description
    QString desc = entry->description;
    if (entry->isGroupHeader || !entry->children.isEmpty()) {
        desc = QString("  ").repeated(entry->depth()) + desc;
    }
    item->setText(1, desc);

    // Type
    item->setText(2, entry->variableType);

    // Value
    item->setText(3, displayValue(entry).toString());

    // Address
    if (entry->isMemoryEntry()) {
        QString addrStr = entry->address;
        for (qint64 off : entry->offsets)
            addrStr += QString(" -> +%1").arg(off, 0, 16);
        item->setText(4, addrStr);
    } else if (entry->isScript()) {
        item->setText(4, "[Script]");
    }

    // Color
    QColor c = entry->resolvedColor();
    if (c.isValid()) {
        item->setForeground(1, c);
    }

    // Build children
    for (CheatEntry* child : entry->children) {
        QTreeWidgetItem* childItem = buildTree(child);
        item->addChild(childItem);
    }

    return item;
}

void CheatTreeWidget::updateValues() {
    if (!m_mm || !m_mm->isAttached()) return;

    m_updating = true;
    for (auto it = m_entryToItem.begin(); it != m_entryToItem.end(); ++it) {
        updateItemValue(it.value(), it.key());
    }
    m_updating = false;
}

void CheatTreeWidget::updateItemValue(QTreeWidgetItem* item, CheatEntry* entry) {
    if (entry->isMemoryEntry() && m_mm && m_mm->isAttached()) {
        quint64 base = m_mm->parseAddress(entry->address);
        if (base == 0) {
            item->setText(3, "[base?]");
            return;
        }
        quint64 addr = m_mm->resolvePointer(base, entry->offsets);
        if (addr == 0) {
            item->setText(3, "[ptr?]");
            return;
        }

        if (entry->variableType == "Float") {
            float val = m_mm->readFloat(addr);
            item->setText(3, QString::number(val, 'f', 4));
        } else if (entry->variableType == "Double") {
            double val = m_mm->readDouble(addr);
            item->setText(3, QString::number(val, 'f', 6));
        } else if (entry->variableType == "4 Bytes") {
            quint32 val = m_mm->readUInt(addr);
            if (entry->showAsSigned)
                item->setText(3, QString::number(static_cast<qint32>(val)));
            else
                item->setText(3, QString::number(val));
        } else if (entry->variableType == "2 Bytes") {
            quint16 val = static_cast<quint16>(m_mm->readShort(addr));
            item->setText(3, QString::number(val));
        } else if (entry->variableType == "1 Byte") {
            quint8 val = m_mm->readByte(addr);
            item->setText(3, QString::number(val));
        }
    }
}

QVariant CheatTreeWidget::displayValue(CheatEntry* entry) const {
    if (!entry->lastStateValue.isEmpty())
        return entry->lastStateValue;
    return "";
}

void CheatTreeWidget::onItemChanged(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (m_updating) return;
}

void CheatTreeWidget::onItemSelectionChanged() {
    auto* item = currentItem();
    if (item && m_itemToEntry.contains(item))
        emit entrySelected(m_itemToEntry[item]);
}

bool CheatTreeWidget::isActivated(CheatEntry* entry) const {
    return m_activated.contains(entry);
}
