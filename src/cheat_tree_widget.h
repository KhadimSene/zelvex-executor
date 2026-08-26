#pragma once

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "cheat_entry.h"
#include "memory_manager.h"

class CheatTreeWidget : public QTreeWidget {
    Q_OBJECT
public:
    explicit CheatTreeWidget(QWidget* parent = nullptr);

    void loadEntries(CheatEntry* root);
    void clearEntries();
    void updateValues();

    void setMemoryManager(MemoryManager* mm) { m_mm = mm; }
    bool isActivated(CheatEntry* entry) const;

signals:
    void cheatActivated(CheatEntry* entry);
    void cheatDeactivated(CheatEntry* entry);
    void entrySelected(CheatEntry* entry);

private slots:
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemSelectionChanged();

private:
    QTreeWidgetItem* buildTree(CheatEntry* entry);
    void updateItemValue(QTreeWidgetItem* item, CheatEntry* entry);
    QVariant displayValue(CheatEntry* entry) const;

    QHash<QTreeWidgetItem*, CheatEntry*> m_itemToEntry;
    QHash<CheatEntry*, QTreeWidgetItem*> m_entryToItem;
    QSet<CheatEntry*> m_activated;
    MemoryManager* m_mm = nullptr;
    bool m_updating = false;
};
