#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <windows.h>
#include <tlhelp32.h>

struct ProcessInfo {
    DWORD pid;
    QString name;
};

class ProcessSelector : public QDialog {
    Q_OBJECT
public:
    explicit ProcessSelector(QWidget* parent = nullptr);
    ProcessInfo selectedProcess() const { return m_selected; }

private slots:
    void refreshProcesses();
    void onConnect();
    void onRefresh();

private:
    void populateList();

    QListWidget* m_listWidget;
    QPushButton* m_refreshBtn;
    QPushButton* m_connectBtn;
    QLabel* m_statusLabel;
    QList<ProcessInfo> m_processes;
    ProcessInfo m_selected{};
};
