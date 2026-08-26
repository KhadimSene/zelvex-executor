#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

class LoadingScreen : public QWidget {
    Q_OBJECT
public:
    explicit LoadingScreen(QWidget* parent = nullptr);

    void setProgress(int value);
    void setStatus(const QString& text);
    void setMaximum(int max) { m_maximum = max; }
    int maximum() const { return m_maximum; }
    int progress() const { return m_progress; }

private:
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_wordmarkLabel;
    QLabel* m_subtitleLabel;
    int m_maximum = 100;
    int m_progress = 0;
};
