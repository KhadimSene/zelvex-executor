#include "loading_screen.h"
#include <QApplication>
#include <QScreen>
#include <QFont>

LoadingScreen::LoadingScreen(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(420, 320);

    setStyleSheet(R"(
        QWidget#loadingWidget {
            background-color: #0b0e12;
            border: 1px solid #1b212b;
            border-radius: 14px;
        }
        QLabel#wordmark {
            font-size: 42px;
            font-weight: 800;
            color: #5AA3FF;
            letter-spacing: 3px;
        }
        QLabel#subtitle {
            font-size: 12px;
            font-weight: 500;
            color: #4a5160;
            letter-spacing: 1px;
        }
        QLabel#statusText {
            font-size: 12px;
            font-weight: 600;
            color: #7b8494;
        }
        QProgressBar {
            background-color: #0a0d11;
            border: 1px solid #1b212b;
            border-radius: 6px;
            text-align: center;
            font-size: 11px;
            font-weight: 600;
            color: #8b93a3;
            min-height: 8px;
            max-height: 8px;
        }
        QProgressBar::chunk {
            background-color: #3D8BFF;
            border-radius: 5px;
        }
    )");

    auto* container = new QWidget(this);
    container->setObjectName("loadingWidget");
    container->setGeometry(0, 0, 420, 320);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(40, 50, 40, 50);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignCenter);

    m_wordmarkLabel = new QLabel("Zelvex");
    m_wordmarkLabel->setObjectName("wordmark");
    m_wordmarkLabel->setAlignment(Qt::AlignCenter);

    m_subtitleLabel = new QLabel("by nyxdev_");
    m_subtitleLabel->setObjectName("subtitle");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setFixedHeight(20);

    layout->addStretch(2);
    layout->addWidget(m_wordmarkLabel, 0, Qt::AlignCenter);
    layout->addSpacing(4);
    layout->addWidget(m_subtitleLabel, 0, Qt::AlignCenter);
    layout->addStretch(1);

    m_progressBar = new QProgressBar();
    m_progressBar->setObjectName("progressBar");
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(8);

    m_statusLabel = new QLabel("Initializing...");
    m_statusLabel->setObjectName("statusText");
    m_statusLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_progressBar, 0, Qt::AlignCenter);
    layout->addSpacing(10);
    layout->addWidget(m_statusLabel, 0, Qt::AlignCenter);
    layout->addStretch(2);

    QFont f = m_wordmarkLabel->font();
    f.setPointSize(42);
    f.setBold(true);
    m_wordmarkLabel->setFont(f);
}

void LoadingScreen::setProgress(int value) {
    m_progress = value;
    m_progressBar->setValue(value);
    QApplication::processEvents();
}

void LoadingScreen::setStatus(const QString& text) {
    m_statusLabel->setText(text);
    QApplication::processEvents();
}
