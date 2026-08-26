#include "process_selector.h"
#include <QHeaderView>

ProcessSelector::ProcessSelector(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Select Process");
    setMinimumSize(500, 500);
    setStyleSheet(R"(
        QDialog { background-color: #0e1116; color: #e6e9ee; }
        QListWidget {
            background-color: #0a0d11; color: #e6e9ee;
            border: 1px solid #232b38; font-size: 13px;
        }
        QListWidget::item:selected { background-color: rgba(61,139,255,0.25); }
        QListWidget::item:hover { background-color: #141920; }
        QPushButton {
            background-color: #3d8bff; color: white;
            border: none; border-radius: 6px;
            padding: 8px 16px; font-size: 12px; font-weight: 600;
        }
        QPushButton:hover { background-color: #5aa3ff; }
        QPushButton#ghostBtn { background-color: transparent; color: #8b93a3; border: 1px solid #232b38; }
        QPushButton#ghostBtn:hover { background-color: #141920; color: white; }
        QLabel { color: #9aa3b2; font-size: 12px; }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* titleLabel = new QLabel("Running Processes:");
    layout->addWidget(titleLabel);

    m_listWidget = new QListWidget();
    m_listWidget->setAlternatingRowColors(true);
    layout->addWidget(m_listWidget);

    auto* btnLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton("Refresh");
    m_refreshBtn->setObjectName("ghostBtn");
    m_connectBtn = new QPushButton("Attach");
    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_connectBtn);
    layout->addLayout(btnLayout);

    m_statusLabel = new QLabel("Ready");
    layout->addWidget(m_statusLabel);

    connect(m_refreshBtn, &QPushButton::clicked, this, &ProcessSelector::onRefresh);
    connect(m_connectBtn, &QPushButton::clicked, this, &ProcessSelector::onConnect);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onConnect();
    });

    populateList();
}

void ProcessSelector::refreshProcesses() {
    populateList();
}

void ProcessSelector::onRefresh() {
    populateList();
}

void ProcessSelector::onConnect() {
    auto* item = m_listWidget->currentItem();
    if (!item) {
        m_statusLabel->setText("No process selected");
        return;
    }
    int row = item->data(Qt::UserRole).toInt();
    if (row >= 0 && row < m_processes.size()) {
        m_selected = m_processes[row];
        accept();
    }
}

void ProcessSelector::populateList() {
    m_listWidget->clear();
    m_processes.clear();

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        m_statusLabel->setText("Failed to enumerate processes");
        return;
    }

    if (Process32FirstW(snap, &pe)) {
        do {
            ProcessInfo info;
            info.pid = pe.th32ProcessID;
            info.name = QString::fromWCharArray(pe.szExeFile);
            if (!info.name.isEmpty()) {
                m_processes.append(info);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    // Sort by name
    std::sort(m_processes.begin(), m_processes.end(),
        [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.name.toLower() < b.name.toLower();
        });

    for (int i = 0; i < m_processes.size(); i++) {
        const ProcessInfo& p = m_processes[i];
        auto* item = new QListWidgetItem(
            QString("%1 [PID: %2]").arg(p.name).arg(p.pid));
        item->setData(Qt::UserRole, i);
        m_listWidget->addItem(item);
    }

    m_statusLabel->setText(QString("Found %1 processes").arg(m_processes.size()));
}
