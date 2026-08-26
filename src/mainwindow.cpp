#include "mainwindow.h"
#include "styles.h"
#include "layout_config.h"
#include "app_icons.h"
#include <QGraphicsOpacityEffect>
#include <QShortcut>
#include <QSet>
#include <QFont>
#include <QClipboard>
#include <QTextBlock>
#include <QLabel>
#include <QFrame>
#include <QSplitter>

#include <windowsx.h>
#include <cmath>
#include <QResizeEvent>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QStatusBar>
#include <QFrame>
#include <QRegularExpression>
#include <QIntValidator>
#include <QScrollBar>
#include <QTabWidget>
#include <QTabBar>
#include <QMenu>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTime>
#include <QTextBlock>

// ── Lua code editor with an integrated line-number gutter (Qt-style) ──

class LuaEditor;

class LuaLineNumberArea : public QWidget {
public:
    explicit LuaLineNumberArea(LuaEditor* editor);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* e) override { e->ignore(); }
private:
    LuaEditor* m_editor;
};

class LuaEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit LuaEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent) {
        m_lineNumberArea = new LuaLineNumberArea(this);
        connect(this, &QPlainTextEdit::blockCountChanged, this, &LuaEditor::updateLineNumberAreaWidth);
        connect(this, &QPlainTextEdit::updateRequest, this, &LuaEditor::updateLineNumberArea);
        connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
            updateLineNumberArea(viewport()->rect(), 0);
            highlightCurrentLine();
            QTextCursor c = textCursor();
            emit cursorInfoChanged(c.blockNumber() + 1, c.positionInBlock() + 1);
        });
        updateLineNumberAreaWidth(0);
    }
    void setLoading(bool loading) { m_loading = loading; }
    bool isLoading() const { return m_loading; }
    int lineNumberAreaWidth() const {
        int digits = 1;
        int max = qMax(1, blockCount());
        while (max >= 10) { max /= 10; ++digits; }
        return 16 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }
signals:
    void cursorInfoChanged(int line, int col);
protected:
    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        QRect cr = contentsRect();
        m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    }
    void lineNumberAreaPaintEvent(QPaintEvent* e) {
        QPainter painter(m_lineNumberArea);
        painter.fillRect(e->rect(), QColor("#0a0d11"));
        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());
        QColor baseColor("#373f4d");
        QColor currentColor("#5AA3FF");
        while (block.isValid() && top <= e->rect().bottom()) {
            if (block.isVisible() && bottom >= e->rect().top()) {
                painter.setPen(blockNumber == textCursor().blockNumber() ? currentColor : baseColor);
                painter.setFont(QFont("Cascadia Code", 8));
                painter.drawText(0, top, m_lineNumberArea->width() - 8, fontMetrics().height(),
                                 Qt::AlignRight | Qt::AlignTop, QString::number(blockNumber + 1));
            }
            block = block.next();
            top = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
            ++blockNumber;
        }
    }
private slots:
    void updateLineNumberAreaWidth(int /*newBlockCount*/) {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    }
    void updateLineNumberArea(const QRect& rect, int dy) {
        if (dy) m_lineNumberArea->scroll(0, dy);
        else m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
        if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
    }
    void highlightCurrentLine() {
        QList<QTextEdit::ExtraSelection> extras;
        QTextEdit::ExtraSelection sel;
        QColor c("#12202f");
        c.setAlpha(70);
        sel.format.setBackground(c);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        extras.append(sel);
        setExtraSelections(extras);
    }
private:
    LuaLineNumberArea* m_lineNumberArea;
    bool m_loading = false;
    friend class LuaLineNumberArea;
};

LuaLineNumberArea::LuaLineNumberArea(LuaEditor* editor)
    : QWidget(editor), m_editor(editor) {
    setObjectName("luaLineArea");
}

QSize LuaLineNumberArea::sizeHint() const {
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LuaLineNumberArea::paintEvent(QPaintEvent* event) {
    m_editor->lineNumberAreaPaintEvent(event);
}

static bool ScriptHasLoop(const QString& code) {
    QString lower = code.toLower();
    return lower.contains("while ") || lower.contains("for ") || lower.contains("wait(");
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowStaysOnTopHint);

    m_cheats = buildCheatDefs();
    m_itemDatabase = buildItemDatabase();

    setWindowIcon(QIcon(":/src/images/logo.png"));
    m_luaExecShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(m_luaExecShortcut, &QShortcut::activated, this, &MainWindow::executeLuaCode);

    setupUI();
    setWindowTitle("Zelvex v3 BETA");
    resize(640, 440);
    setMinimumSize(560, 380);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshTimer);

    m_healthTimer = new QTimer(this);
    m_healthTimer->setInterval(3000);
    connect(m_healthTimer, &QTimer::timeout, this, &MainWindow::checkProcessHealth);

    // Show the window so it paints, then do init in deferred batches
    // via a singleShot timer  - the event loop stays responsive throughout
    show();
    m_loadingScreen = new LoadingScreen(nullptr);
    QRect screenGeom = QApplication::primaryScreen()->availableGeometry();
    m_loadingScreen->move((screenGeom.width() - m_loadingScreen->width()) / 2,
                          (screenGeom.height() - m_loadingScreen->height()) / 2);
    m_loadingScreen->show();

    QTimer::singleShot(100, this, [this]() {
        initLoadingStages();
    });
}

MainWindow::~MainWindow() {
    m_refreshTimer->stop();
    m_healthTimer->stop();
    stopRandomTeleport();
    for (auto it = m_injections.begin(); it != m_injections.end(); ++it) {
        if (it->remoteThread) {
            WaitForSingleObject(it->remoteThread, 500);
            CloseHandle(it->remoteThread);
        }
    }
    m_mm.restoreAllPatches();
    m_mm.detach();
}

// ── Window Shape ──

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), 12, 12);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

// ── Window Dragging ──

void MainWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_dragging = true;
        event->accept();
    }
}
void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && m_dragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}
void MainWindow::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    event->accept();
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg && msg->message == WM_HOTKEY) {
            int slot = -1;
            if (msg->wParam == HOTKEY_ID_1) slot = 0;
            else if (msg->wParam == HOTKEY_ID_2) slot = 1;
            else if (msg->wParam == HOTKEY_ID_3) slot = 2;
            if (slot >= 0 && m_hotkeyEdits[slot]) {
                QString code = m_hotkeyEdits[slot]->text().trimmed();
                if (!code.isEmpty()) {
                    appendLuaOutput(QString("[hotkey Ctrl+Alt+%1] ").arg(slot + 1));
                    executeLuaViaDll(code);
                }
            }
            if (result) *result = 0;
            return true;
        }
    }
    return false;
}

// Global hotkeys Ctrl+Alt+1/2/3 -> run the Lua one-liner from the Misc tab.
// Registered system-wide so they work while the game has focus.
void MainWindow::registerHotkeys() {
    if (m_hotkeysRegistered) return;
    HWND h = reinterpret_cast<HWND>(winId());
    if (!h) return;
    RegisterHotKey(h, HOTKEY_ID_1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '1');
    RegisterHotKey(h, HOTKEY_ID_2, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '2');
    RegisterHotKey(h, HOTKEY_ID_3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '3');
    m_hotkeysRegistered = true;
}

// ── Language Support ──

void MainWindow::setLanguage(Language newLang) {
    m_currentLanguage = newLang;
    const auto& L = lang(newLang);

    // Update language button states
    for (int i = 0; i < 6; i++) {
        if (m_langButtons[i]) {
            m_langButtons[i]->setChecked(
                m_langButtons[i]->property("lang").toInt() == static_cast<int>(newLang));
        }
    }

    // Update sidebar tab icon labels and text labels
    auto tabDefs = UILayout::getTabDefs();
    setTabIconStates(m_activeTab);
    for (int i = 0; i < m_tabIcons.size() && i < tabDefs.size() && i < 9; i++) {
        if (i < m_tabLabels.size()) {
            m_tabLabels[i]->setText(QString::fromUtf8(L.tabLabels[i]));
        }
    }

    // Update sidebar status text
    if (m_statusLabel) {
        m_statusLabel->setText(QString::fromUtf8(m_mm.isAttached() ? L.sidebarStatus : L.sidebarOffline));
    }
    if (m_attachBtn) {
        m_attachBtn->setText(QString::fromUtf8(L.sidebarAttach));
    }

    // Reload current tab to apply new language
    if (m_activeTab >= 0) {
        m_tabLoaded[m_activeTab] = false;
        if (m_activeTab == 7) { m_itemsTabLoaded = false; }
        if (m_activeTab == 8) { m_luaTabLoaded = false; }
        if (m_activeTab == 7 || m_activeTab == 8) {
            m_luaStatusLabel = nullptr;
            m_luaCodeEditor = nullptr;
            m_luaConsole = nullptr;
        }
        loadEntries(m_activeTab);
    }
}

QString MainWindow::trTab(const QString& english) const {
    const auto& en = lang(Language::English);
    const auto& cur = lang(m_currentLanguage);
    for (int i = 0; i < 9; i++) {
        if (english == QString::fromUtf8(en.tabLabels[i]))
            return QString::fromUtf8(cur.tabLabels[i]);
    }
    return english;
}

QString MainWindow::trSection(const QString& english) const {
    const auto& en = lang(Language::English);
    const auto& cur = lang(m_currentLanguage);
    for (int i = 0; i < 12; i++) {
        if (english == QString::fromUtf8(en.sectionHeaders[i]))
            return QString::fromUtf8(cur.sectionHeaders[i]);
    }
    return english;
}

QString MainWindow::trCheatName(int cheatId) const {
    const auto& L = lang(m_currentLanguage);
    for (int i = 0; i < L.cheatCount; i++) {
        if (L.cheats[i].id == cheatId) return QString::fromUtf8(L.cheats[i].name);
    }
    for (const auto& c : m_cheats)
        if (c.id == cheatId) return c.name;
    return "";
}

QString MainWindow::trCheatDesc(int cheatId) const {
    const auto& L = lang(m_currentLanguage);
    for (int i = 0; i < L.cheatCount; i++) {
        if (L.cheats[i].id == cheatId) return QString::fromUtf8(L.cheats[i].desc);
    }
    for (const auto& c : m_cheats)
        if (c.id == cheatId) return c.subtitle;
    return "";
}

const char* MainWindow::trUI(const char* key) const {
    const auto& L = lang(m_currentLanguage);
    if (key == QByteArray("status")) return L.sidebarStatus;
    if (key == QByteArray("attach")) return L.sidebarAttach;
    if (key == QByteArray("offline")) return L.sidebarOffline;
    if (key == QByteArray("beta")) return L.betaWarning;
    if (key == QByteArray("terrain_warn")) return L.terrainWarning;
    if (key == QByteArray("items_warn")) return L.itemsWarning;
    if (key == QByteArray("lang_label")) return L.langLabel;
    return key;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    auto* w = qobject_cast<QWidget*>(obj);
    if (w && w->objectName() == "tabContainer") {
        int idx = w->property("tabIndex").toInt();
        if (idx >= 0 && idx < m_tabIcons.size()) {
            auto* iconLabel = m_tabIcons[idx];
            bool isActive = (idx == m_activeTab);

            if (event->type() == QEvent::Enter && !isActive) {
                iconLabel->setProperty("hover", true);
                updateTabIcon(idx);
            } else if (event->type() == QEvent::Leave && !isActive) {
                iconLabel->setProperty("hover", false);
                updateTabIcon(idx);
            } else if (event->type() == QEvent::MouseButtonPress) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    switchTab(idx);
                    return true;
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// ── UI Setup ──

void MainWindow::setupUI() {
    auto* central = new QWidget();
    central->setObjectName("centralWidget");
    setCentralWidget(central);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Header (slim overlay titlebar) ──
    auto* header = new QWidget();
    header->setObjectName("headerWidget");
    header->setFixedHeight(28);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(10, 0, 8, 0);
    headerLayout->setSpacing(6);

    auto* markLabel = new QLabel();
    {
        QPixmap logo(":/src/images/logo.png");
        if (!logo.isNull())
            markLabel->setPixmap(logo.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            markLabel->setPixmap(AppIcons::labelPixmap(AppIcons::Id::Brand, QColor("#5AA3FF"), 16));
    }
    markLabel->setFixedSize(18, 18);
    markLabel->setStyleSheet("background: transparent; border: none;");
    headerLayout->addWidget(markLabel);

    auto* titleLabel = new QLabel("ZELVEX");
    titleLabel->setStyleSheet("font-size: 11px; font-weight: 800; color: #e6e9ee; letter-spacing: 1px; background: transparent; border: none;");
    headerLayout->addWidget(titleLabel);

    m_headerPidLabel = new QLabel("GAME OFFLINE");
    m_headerPidLabel->setObjectName("statusChip");
    m_headerPidLabel->setFixedHeight(18);
    headerLayout->addSpacing(4);
    headerLayout->addWidget(m_headerPidLabel);
    headerLayout->addStretch();

    QTimer::singleShot(0, this, [this]() { updateStatusVisual(m_mm.isAttached()); });

    auto* pinBtn = new QPushButton("P");
    pinBtn->setObjectName("windowBtn");
    pinBtn->setCheckable(true);
    pinBtn->setChecked(true);
    pinBtn->setFixedSize(26, 22);
    pinBtn->setToolTip("Pin window on top of the game (off = normal window)");
    connect(pinBtn, &QPushButton::toggled, this, [this](bool on) {
        setWindowFlag(Qt::WindowStaysOnTopHint, on);
        show();
    });
    headerLayout->addWidget(pinBtn);

    auto* minBtn = new QPushButton("-");
    minBtn->setObjectName("windowBtn");
    minBtn->setFixedSize(26, 22);
    connect(minBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    headerLayout->addWidget(minBtn);

    auto* closeBtn = new QPushButton("X");
    closeBtn->setObjectName("windowBtnClose");
    closeBtn->setFixedSize(26, 22);
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::close);
    headerLayout->addWidget(closeBtn);

    mainLayout->addWidget(header);

    // ── Body ──
    auto* bodyWidget = new QWidget();
    auto* bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // ── Sidebar ──
    m_sidebar = new QWidget();
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(SIDEBAR_COLLAPSED);
    m_sidebar->setMinimumWidth(SIDEBAR_COLLAPSED);
    m_sidebar->setMaximumWidth(SIDEBAR_COLLAPSED);
    auto* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(0, 12, 0, 12);
    sidebarLayout->setSpacing(3);

    auto* headerBox = new QWidget();
    headerBox->setObjectName("sidebarHeaderBox");
    auto* headerBoxLayout = new QVBoxLayout(headerBox);
    headerBoxLayout->setContentsMargins(0, 0, 0, 0);
    headerBoxLayout->setSpacing(0);
    m_sidebarHeader = new QLabel("ZELVEX");
    m_sidebarHeader->setObjectName("sidebarTitle");
    headerBoxLayout->addWidget(m_sidebarHeader);
    m_sidebarSub = new QLabel("EXECUTOR v4.0");
    m_sidebarSub->setObjectName("sidebarSubtitle");
    headerBoxLayout->addWidget(m_sidebarSub);
    sidebarLayout->addWidget(headerBox);

    auto* sepLine = new QFrame();
    sepLine->setObjectName("sidebarSep");
    sepLine->setFrameShape(QFrame::HLine);
    sidebarLayout->addWidget(sepLine);

    auto* tabScroll = new QScrollArea();
    tabScroll->setObjectName("sidebarScroll");
    tabScroll->setWidgetResizable(true);
    tabScroll->setFrameShape(QFrame::NoFrame);
    tabScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* tabList = new QWidget();
    tabList->setObjectName("sidebarTabList");
    tabList->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* tabListLayout = new QVBoxLayout(tabList);
    tabListLayout->setContentsMargins(0, 0, 0, 0);
    tabListLayout->setSpacing(3);

    auto tabDefs = UILayout::getTabDefs();
    for (int i = 0; i < tabDefs.size(); i++) {
        auto* btnContainer = new QWidget();
        btnContainer->setObjectName("tabContainer");
        btnContainer->setMinimumHeight(40);
        btnContainer->setMaximumHeight(40);
        auto* btnLayout = new QHBoxLayout(btnContainer);
        btnLayout->setContentsMargins(8, 0, 8, 0);
        btnLayout->setSpacing(10);

        auto* iconLabel = new QLabel();
        iconLabel->setObjectName("tabIcon");
        iconLabel->setFixedHeight(28);
        iconLabel->setFixedWidth(SIDEBAR_COLLAPSED - 32);
        iconLabel->setAlignment(Qt::AlignCenter);
        if (tabDefs[i].logo) {
            QPixmap logo(":/src/images/logo.png");
            if (!logo.isNull())
                iconLabel->setPixmap(logo.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else
                iconLabel->setPixmap(AppIcons::labelPixmap(AppIcons::Id::Brand, QColor("#3D8BFF"), 24));
        } else {
            iconLabel->setPixmap(AppIcons::labelPixmap(tabDefs[i].icon, QColor("#9AA3B2"), 24));
        }
        btnLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);

        auto* textLabel = new QLabel(QString::fromUtf8(lang(m_currentLanguage).tabLabels[i]));
        textLabel->setObjectName("tabText");
        btnLayout->addWidget(textLabel, 1, Qt::AlignVCenter);

        btnContainer->setProperty("tabIndex", i);
        btnContainer->installEventFilter(this);
        btnContainer->setCursor(Qt::PointingHandCursor);
        btnContainer->setToolTip(QString("%1  ·  %2")
            .arg(tabDefs[i].label)
            .arg(QString::fromUtf8(lang(m_currentLanguage).tabLabels[i])));

        m_tabIcons.append(iconLabel);
        m_tabLabels.append(textLabel);

        tabListLayout->addWidget(btnContainer);
    }

    tabScroll->setWidget(tabList);
    sidebarLayout->addWidget(tabScroll, 1);

    auto* statusContainer = new QWidget();
    statusContainer->setObjectName("statusContainer");
    auto* statusLayout = new QVBoxLayout(statusContainer);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    statusLayout->setSpacing(6);

    auto* statusRow = new QWidget();
    auto* statusRowLayout = new QHBoxLayout(statusRow);
    statusRowLayout->setContentsMargins(0, 0, 0, 0);
    statusRowLayout->setSpacing(7);

    m_statusDot = new QLabel();
    m_statusDot->setObjectName("statusDot");
    m_statusDot->setFixedSize(8, 8);
    m_statusDot->setStyleSheet("background-color: #f85149; border-radius: 4px; border: none;");
    statusRowLayout->addWidget(m_statusDot, 0, Qt::AlignVCenter);

    m_statusLabel = new QLabel("GAME OFFLINE");
    m_statusLabel->setObjectName("statusLabel");
    statusRowLayout->addWidget(m_statusLabel, 0, Qt::AlignVCenter);
    statusRowLayout->addStretch();
    statusLayout->addWidget(statusRow);

    m_attachBtn = new QPushButton("  ATTACH");
    m_attachBtn->setObjectName("attachButton");
    m_attachBtn->setFixedHeight(38);
    connect(m_attachBtn, &QPushButton::clicked, this, &MainWindow::onAttachClicked);
    statusLayout->addWidget(m_attachBtn);

    sidebarLayout->addWidget(statusContainer);

    bodyLayout->addWidget(m_sidebar);

    // ── Content ──
    auto* rightWidget = new QWidget();
    rightWidget->setObjectName("contentStack");
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_stack = new QStackedWidget();
    m_stack->setObjectName("contentStack");

    for (int i = 0; i < tabDefs.size(); i++) {
        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        auto* page = new QWidget();
        page->setObjectName("pageContent");
        page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(8);
        layout->addStretch();
        scroll->setWidget(page);
        m_stack->addWidget(scroll);
    }

    rightLayout->addWidget(m_stack);
    bodyLayout->addWidget(rightWidget, 1);
    mainLayout->addWidget(bodyWidget, 1);

    m_tabLoaded.resize(tabDefs.size());
    m_tabLoaded.fill(false);

    updateSidebarBody(false);
    switchTab(8);
}

// ── Widget Builders ──

QWidget* MainWindow::createPageHeader(const QString& title) {
    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 10, 0, 0);
    layout->setSpacing(6);

    auto* label = new QLabel(title.toUpper());
    label->setObjectName("sectionHeader");
    layout->addWidget(label);

    auto* line = new QFrame();
    line->setObjectName("sectionLine");
    line->setFrameShape(QFrame::HLine);
    layout->addWidget(line);
    return container;
}

QWidget* MainWindow::createToggleRow(const CheatDef* cheat, const QString& subtitle) {
    auto* widget = new QWidget();
    widget->setObjectName("cheatCard");
    widget->setMinimumHeight(52);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(0);

    auto* textWidget = new QWidget();
    auto* textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    auto* nameLabel = new QLabel(cheat ? trCheatName(cheat->id) : "Unknown");
    nameLabel->setObjectName("toggleName");
    textLayout->addWidget(nameLabel);

    if (!subtitle.isEmpty()) {
        auto* subLabel = new QLabel(cheat ? trCheatDesc(cheat->id) : subtitle);
        subLabel->setObjectName("toggleSubtitle");
        textLayout->addWidget(subLabel);
    }

    layout->addWidget(textWidget, 1);

    auto* pill = new SwitchButton();
    pill->setObjectName("togglePill");
    layout->addWidget(pill, 0, Qt::AlignRight | Qt::AlignVCenter);

    if (cheat) {
        ToggleItem item;
        item.cheat = cheat;
        item.pill = pill;
        m_toggles.append(item);
    }

    connect(pill, &QAbstractButton::clicked, this, &MainWindow::onTogglePillClicked);
    return widget;
}

QWidget* MainWindow::createActionRow(const CheatDef* cheat) {
    auto* widget = new QWidget();
    widget->setObjectName("cheatCard");
    widget->setMinimumHeight(52);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(0);

    auto* textWidget = new QWidget();
    auto* textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    auto* nameLabel = new QLabel(cheat ? trCheatName(cheat->id) : "Unknown");
    nameLabel->setObjectName("toggleName");
    textLayout->addWidget(nameLabel);

    if (cheat && cheat->subtitle) {
        auto* subLabel = new QLabel(trCheatDesc(cheat->id));
        subLabel->setObjectName("toggleSubtitle");
        textLayout->addWidget(subLabel);
    }

    layout->addWidget(textWidget, 1);

    auto* btn = new QPushButton("OPEN");
    btn->setObjectName("actionButton");
    btn->setFixedSize(72, 30);
    layout->addWidget(btn, 0, Qt::AlignRight | Qt::AlignVCenter);

    if (cheat) {
        ActionItem item;
        item.cheat = cheat;
        item.button = btn;
        m_actions.append(item);
    }

    connect(btn, &QPushButton::clicked, this, [this, cheat]() {
        if (cheat) applyCheat(*cheat);
    });
    return widget;
}

static bool isDisabledCheat(int id) {
    return id == 999001;
}

QWidget* MainWindow::createDisabledRow(const CheatDef* cheat) {
    auto* widget = new QWidget();
    widget->setObjectName("cheatCard");
    widget->setMinimumHeight(52);
    widget->setEnabled(false);
    widget->setStyleSheet(widget->styleSheet() + " opacity: 0.5;");
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(0);

    auto* textWidget = new QWidget();
    auto* textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    auto* nameLabel = new QLabel(cheat ? trCheatName(cheat->id) : "Unknown");
    nameLabel->setObjectName("toggleName");
    nameLabel->setEnabled(false);
    textLayout->addWidget(nameLabel);

    auto* subLabel = new QLabel("Unstable  - coming in a future update");
    subLabel->setObjectName("toggleSubtitle");
    subLabel->setEnabled(false);
    textLayout->addWidget(subLabel);

    layout->addWidget(textWidget, 1);

    auto* lockLabel = new QLabel("🔒");
    lockLabel->setStyleSheet("font-size: 16px; background: transparent; border: none; opacity: 0.5;");
    lockLabel->setEnabled(false);
    layout->addWidget(lockLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    return widget;
}

QWidget* MainWindow::createInputRow(const CheatDef* cheat) {
    auto* widget = new QWidget();
    widget->setObjectName("cheatCard");
    widget->setMinimumHeight(52);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(0);

    auto* label = new QLabel(cheat ? trCheatName(cheat->id) : "Unknown");
    label->setObjectName("toggleName");
    layout->addWidget(label, 1);

    auto* input = new QLineEdit();
    input->setObjectName("valueInput");
    input->setAlignment(Qt::AlignRight);
    input->setReadOnly(false);
    input->setFixedWidth(140);
    input->setPlaceholderText("...");

    layout->addWidget(input, 0, Qt::AlignRight | Qt::AlignVCenter);

    if (cheat) {
        InputItem item;
        item.cheat = cheat;
        item.input = input;
        m_inputs.append(item);

        connect(input, &QLineEdit::returnPressed, this, [this, cheat]() {
            for (const InputItem& ii : m_inputs) {
                if (ii.cheat == cheat && ii.input) {
                    writePointerValue(*cheat, ii.input->text());
                    break;
                }
            }
        });
        connect(input, &QLineEdit::editingFinished, this, [this, cheat]() {
            for (const InputItem& ii : m_inputs) {
                if (ii.cheat == cheat && ii.input) {
                    writePointerValue(*cheat, ii.input->text());
                    break;
                }
            }
        });
    }

    return widget;
}

QWidget* MainWindow::createComboRow(const QString& label, QComboBox* combo) {
    auto* widget = new QWidget();
    widget->setObjectName("cheatCard");
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(0);

    auto* lbl = new QLabel(label);
    lbl->setObjectName("toggleName");

    layout->addWidget(lbl, 1);
    layout->addWidget(combo, 0, Qt::AlignRight | Qt::AlignVCenter);

    return widget;
}

// ── Toggle Pill Handler ──

void MainWindow::onTogglePillClicked() {
    auto* pill = qobject_cast<SwitchButton*>(sender());
    if (!pill) return;

    const CheatDef* cheat = nullptr;
    for (const ToggleItem& ti : m_toggles) {
        if (ti.pill == pill) { cheat = ti.cheat; break; }
    }
    if (!cheat) return;

    if (!m_mm.isAttached()) {
        pill->setChecked(false);
        m_statusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).attachToGameFirst));
        return;
    }

    if (pill->isChecked())
        applyCheat(*cheat);
    else
        restoreCheat(*cheat);
}

// ── Process Health Check ──

void MainWindow::checkProcessHealth() {
    if (m_mm.isAttached()) {
        if (!MemoryManager::isProcessRunning("MiniGameApp.exe")) {
            m_refreshTimer->stop();
            for (auto it = m_injections.begin(); it != m_injections.end(); ++it) {
                if (it->remoteThread) { WaitForSingleObject(it->remoteThread, 500); CloseHandle(it->remoteThread); }
            }
            m_mm.restoreAllPatches();
            m_injections.clear();
            m_mm.detach();
            m_gameWindow = nullptr;
            m_resolvedAddrCache.clear();
            m_resolveFailCount.clear();
            m_attachBtn->setText("ATTACH");
            m_statusLabel->setText("Disconnected");
            updateStatusVisual(false);
            m_statusDot->setVisible(true);
            m_statusLabel->setVisible(true);
            updateGeneralAttachBtn();
            for (const ToggleItem& ti : m_toggles) {
                if (ti.pill) ti.pill->setChecked(false);
            }
        }
    } else {
        if (MemoryManager::isProcessRunning("MiniGameApp.exe"))
            onAttachClicked();
    }
}

// ── General Tab Attach Button ──

void MainWindow::updateGeneralAttachBtn() {
    if (!m_generalAttachBtn) return;

    const auto& L = lang(m_currentLanguage);
    QString gameName = "Mini World";
    QString processName = "MiniGameApp.exe";

    if (m_mm.isAttached()) {
        DWORD pid = m_mm.processId();
        QString name = m_mm.processName();
        m_generalAttachBtn->setText(QString("%1 %2 [%3]").arg(QString::fromUtf8(L.attachedTo)).arg(name.isEmpty() ? processName : name).arg(pid));
        m_generalAttachBtn->setEnabled(false);
        m_generalAttachBtn->setStyleSheet(
            "QPushButton { background-color: #11151b; color: #5f6875; border: 1px solid #232b38;"
            "border-radius: 6px; font-size: 12px; font-weight: 600; }");
    } else {
        DWORD pid = MemoryManager::getProcessIdByName(processName);
        if (pid != 0) {
            m_generalAttachBtn->setText(QString("%1 %2 [%3]").arg(QString::fromUtf8(L.attachTo)).arg(gameName).arg(pid));
            m_generalAttachBtn->setEnabled(true);
            m_generalAttachBtn->setStyleSheet(
                "QPushButton { background-color: #3d8bff; color: white; border: none;"
                "border-radius: 6px; font-size: 12px; font-weight: 600;"
                "padding: 10px; }"
                "QPushButton:hover { background-color: #5aa3ff; }"
                "QPushButton:disabled { background-color: #11151b; color: #5f6875; }");
        } else {
            m_generalAttachBtn->setText(QString("%1 %2").arg(QString::fromUtf8(L.noProcessFound)).arg(gameName));
            m_generalAttachBtn->setEnabled(false);
            m_generalAttachBtn->setStyleSheet(
                "QPushButton { background-color: rgba(248,81,73,0.10); color: #f85149; border: 1px solid rgba(248,81,73,0.35);"
                "border-radius: 6px; font-size: 12px; font-weight: 600; }");
        }
    }
}

// ── Detect Stale Patches (from CE sessions) ──

void MainWindow::detectStalePatches() {
    if (!m_mm.isAttached()) return;

    int staleCount = 0;
    int scanCount = 0;
    for (const CheatDef& cheat : m_cheats) {
        if (cheat.type != CheatType::TOGGLE_AOB_JMP && cheat.type != CheatType::TOGGLE_AOB_DB)
            continue;

        if (cheat.symbolName && cheat.module) {
            quint64 symAddr = m_mm.resolvePdbSymbol(cheat.module, cheat.symbolName);
            if (symAddr != 0) {
                quint64 addr = symAddr + cheat.symbolOffset;
                QByteArray currentBytes = m_mm.readBytes(addr, 2);
                QByteArray expectedBytes;
                QStringList parts = QString(cheat.enableBytes).split(' ', Qt::SkipEmptyParts);
                for (const QString& p : parts)
                    expectedBytes.append(static_cast<char>(p.toUInt(nullptr, 16)));
                if (!expectedBytes.isEmpty() && currentBytes.left(expectedBytes.size()) == expectedBytes)
                    staleCount++;
            }
            continue;
        }

        if (!cheat.aob || !cheat.module) continue;

        QByteArray pattern, mask;
        pattern = parseAobPattern(cheat.aob, mask);
        quint64 addr = m_mm.aobScanSingle(cheat.module, pattern, mask);

        if (addr == 0) {
            staleCount++;
        }

        scanCount++;
        if (scanCount % 3 == 0) {
            QCoreApplication::processEvents();
        }
    }
    if (staleCount > 0) {
        m_statusLabel->setText(QString("Warning: %1 stale patch(es) detected. Restart the game.").arg(staleCount));
    }
}

// ── Pre-scan all AOB patterns on attach for instant toggle lookups ──

void MainWindow::preScanAobs() {
    if (!m_mm.isAttached()) return;

    QList<QPair<QString, QPair<QByteArray, QByteArray>>> scans;
    for (const CheatDef& cheat : m_cheats) {
        if (cheat.aob && cheat.module) {
            QByteArray pattern, mask;
            pattern = parseAobPattern(cheat.aob, mask);
            scans.append({QString(cheat.module), {pattern, mask}});
        }
    }
    m_mm.preScanAllAobs(scans);

    for (const CheatDef& cheat : m_cheats) {
        if (cheat.symbolName && cheat.module) {
            m_mm.resolvePdbSymbol(cheat.module, cheat.symbolName);
        }
    }
}

// ── Cheat Entry Finder ──

const CheatDef* MainWindow::findCheatById(int id) {
    for (const CheatDef& c : m_cheats) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

// ── Load Entries ──

void MainWindow::loadEntries(int onlyTab) {
    m_toggles.clear();
    m_actions.clear();
    m_inputs.clear();
    m_conditionCombo = nullptr;

    auto findLayout = [&](int index) -> QVBoxLayout* {
        auto* scroll = qobject_cast<QScrollArea*>(m_stack->widget(index));
        if (!scroll) return nullptr;
        auto* page = scroll->widget();
        if (!page) return nullptr;
        return qobject_cast<QVBoxLayout*>(page->layout());
    };

    auto clearLayout = [](QVBoxLayout* layout) {
        if (!layout) return;
        while (layout->count() > 1) {
            QLayoutItem* item = layout->takeAt(0);
            if (item) { if (item->widget()) delete item->widget(); delete item; }
        }
    };

    auto tabDefs = UILayout::getTabDefs();

// On initial load (onlyTab < 0), init flags and build nothing extra -
// tabs build lazily on first click. Never wipe flags for already-built
// tabs (the Lua page is built by setupUI's switchTab, and wiping here
// would clear + blank it via the loadLuaTab() guard).
bool firstInit = (onlyTab < 0);
    if (firstInit) {
        if (m_tabLoaded.size() != tabDefs.size()) {
            m_tabLoaded.clear();
            m_tabLoaded.resize(tabDefs.size(), false);
        }
        onlyTab = 0;
    }

    if (onlyTab >= 0) {
        int tab = onlyTab;
        auto* layout = findLayout(tab);
        if (!layout) return;
        clearLayout(layout);

        int headerIdx = 0;
        QList<int> inputIds, toggleIds, comboIds, actionIds;

        for (int id : tabDefs[tab].cheatIds) {
            const CheatDef* c = findCheatById(id);
            if (!c) continue;
            switch (c->type) {
                case CheatType::INPUT_FLOAT:
                case CheatType::INPUT_INT:
                    inputIds.append(id); break;
                case CheatType::COMBO_INT:
                    comboIds.append(id); break;
                case CheatType::ACTION_WIN32:
                case CheatType::ACTION_AOB_DB:
                case CheatType::ACTION_AOB_JMP:
                    actionIds.append(id); break;
                default:
                    toggleIds.append(id); break;
            }
        }

        // Render combat tabs with interleaved sections
        if (tab == 5) { // Combat tab
            QList<int> autoClickToggles, spectatorToggles, offensiveToggles;
            QList<int> spectatorActions;
            for (int id : toggleIds) {
                const CheatDef* c = findCheatById(id);
                if (c && (c->id == 1337197721 || c->id == 1337197722))
                    autoClickToggles.append(id);
                else if (c && (c->id == 1337212286))
                    spectatorToggles.append(id);
                else
                    offensiveToggles.append(id);
            }
            for (int id : actionIds) {
                const CheatDef* c = findCheatById(id);
                if (c && (c->id == 1807635788))
                    spectatorActions.append(id);
                else
                    offensiveToggles.append(id);
            }
            // Auto Click section
            if (!autoClickToggles.isEmpty() && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : autoClickToggles) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createToggleRow(c, c->subtitle));
            }
            // Spectator section (toggle + action)
            bool hasSpecSection = !spectatorToggles.isEmpty() || !spectatorActions.isEmpty();
            if (hasSpecSection && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : spectatorToggles) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createToggleRow(c, c->subtitle));
            }
            for (int id : spectatorActions) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createActionRow(c));
            }
            // Offensive section
            if (!offensiveToggles.isEmpty() && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : offensiveToggles) {
                const CheatDef* c = findCheatById(id);
                if (!c) continue;
                if (isDisabledCheat(c->id))
                    layout->insertWidget(layout->count() - 1, createDisabledRow(c));
                else
                    layout->insertWidget(layout->count() - 1, createToggleRow(c, c->subtitle));
            }
            // Native Zelvex PvP section (DLL commands via shared memory, async)
            layout->insertWidget(layout->count() - 1, createPageHeader("Zelvex Native PvP (DLL)"));
            auto nativeCmdRow = [this](const QString& title, const QString& sub,
                                      const QList<QPair<QString, QString>>& actions) -> QWidget* {
                auto* widget = new QWidget();
                widget->setObjectName("cheatCard");
                widget->setMinimumHeight(52);
                auto* row = new QHBoxLayout(widget);
                row->setContentsMargins(16, 10, 16, 10);
                row->setSpacing(8);
                auto* textW = new QWidget();
                auto* textL = new QVBoxLayout(textW);
                textL->setContentsMargins(0, 0, 0, 0);
                textL->setSpacing(2);
                auto* nameL = new QLabel(title);
                nameL->setObjectName("toggleName");
                textL->addWidget(nameL);
                if (!sub.isEmpty()) {
                    auto* subL = new QLabel(sub);
                    subL->setObjectName("toggleSubtitle");
                    textL->addWidget(subL);
                }
                row->addWidget(textW, 1);
                for (const auto& a : actions) {
                    auto* b = new QPushButton(a.first);
                    b->setObjectName("ghostButton");
                    b->setFixedHeight(28);
                    QString cmd = a.second;
                    connect(b, &QPushButton::clicked, this, [this, cmd]() {
                        executeLuaViaDll(cmd);
                    });
                    row->addWidget(b);
                }
                return widget;
            };
            layout->insertWidget(layout->count() - 1,
                nativeCmdRow("Kill Aura", "Auto-attack nearby players while targeted",
                    {{"Aura 1", "native.killAura(1)"},
                     {"Aura 2", "native.killAura(2)"},
                     {"OFF", "native.killAura(0)"}}));
            layout->insertWidget(layout->count() - 1,
                nativeCmdRow("Mount All", "Ride every nearby mountable animal",
                    {{"ON", "native.mountAll(1)"},
                     {"OFF", "native.mountAll(0)"}}));
            layout->insertWidget(layout->count() - 1,
                nativeCmdRow("Mine All", "Auto-mine ores/clay nearby",
                    {{"ON", "native.mineAll(1)"},
                     {"OFF", "native.mineAll(0)"}}));
            layout->insertWidget(layout->count() - 1,
                nativeCmdRow("KillAll Host", "Force-finish nearby host monsters",
                    {{"ON", "native.killAllHost(1)"},
                     {"OFF", "native.killAllHost(0)"}}));
            layout->insertWidget(layout->count() - 1,
                nativeCmdRow("Aim Bot", "Auto-lock aim on nearest enemy (range 3000)",
                    {{"ON", "native.aimbot(1)"},
                     {"OFF", "native.aimbot(0)"}}));
            layout->insertWidget(layout->count() - 1,
                nativeCmdRow("Room Mass Effects", "One-shot the whole room (you excluded by default)",
                    {{"ALL DIE", "native.allDie()"},
                     {"ALL DANCE", "native.allDance()"}}));
            // RoomKick row: player id input + KICK button
            {
                auto* widget = new QWidget();
                widget->setObjectName("cheatCard");
                widget->setMinimumHeight(52);
                auto* row = new QHBoxLayout(widget);
                row->setContentsMargins(16, 10, 16, 10);
                row->setSpacing(8);
                auto* textW = new QWidget();
                auto* textL = new QVBoxLayout(textW);
                textL->setContentsMargins(0, 0, 0, 0);
                textL->setSpacing(2);
                auto* nameL = new QLabel("Room Kick");
                nameL->setObjectName("toggleName");
                textL->addWidget(nameL);
                auto* subL = new QLabel("Request-kick the player with this UID (get IDs via state() or Player radar)");
                subL->setObjectName("toggleSubtitle");
                textL->addWidget(subL);
                row->addWidget(textW, 1);
                auto* idEdit = new QLineEdit();
                idEdit->setObjectName("toolCombo");
                idEdit->setPlaceholderText("player uid");
                idEdit->setFixedHeight(28);
                idEdit->setFixedWidth(130);
                row->addWidget(idEdit);
                auto* kickBtn = new QPushButton("KICK");
                kickBtn->setObjectName("stopButton");
                kickBtn->setFixedHeight(28);
                connect(kickBtn, &QPushButton::clicked, this, [this, idEdit]() {
                    QString idStr = idEdit->text().trimmed();
                    if (idStr.isEmpty()) {
                        appendLuaOutput("Enter the player UID first (run state() to list players).");
                        return;
                    }
                    executeLuaViaDll("native.roomKick(" + idStr + ")");
                });
                row->addWidget(kickBtn);
                layout->insertWidget(layout->count() - 1, widget);
            }
            // KillPlayer row: player id input + kill button
            {
                auto* widget = new QWidget();
                widget->setObjectName("cheatCard");
                widget->setMinimumHeight(52);
                auto* row = new QHBoxLayout(widget);
                row->setContentsMargins(16, 10, 16, 10);
                row->setSpacing(8);
                auto* textW = new QWidget();
                auto* textL = new QVBoxLayout(textW);
                textL->setContentsMargins(0, 0, 0, 0);
                textL->setSpacing(2);
                auto* nameL = new QLabel("Kill Player");
                nameL->setObjectName("toggleName");
                textL->addWidget(nameL);
                auto* subL = new QLabel("Teleport-kill the player with this UID (copy from state())");
                subL->setObjectName("toggleSubtitle");
                textL->addWidget(subL);
                row->addWidget(textW, 1);
                auto* idEdit = new QLineEdit();
                idEdit->setObjectName("toolCombo");
                idEdit->setPlaceholderText("player uid");
                idEdit->setFixedHeight(28);
                idEdit->setFixedWidth(130);
                row->addWidget(idEdit);
                auto* killBtn = new QPushButton("KILL");
                killBtn->setObjectName("stopButton");
                killBtn->setFixedHeight(28);
                connect(killBtn, &QPushButton::clicked, this, [this, idEdit]() {
                    QString idStr = idEdit->text().trimmed();
                    if (idStr.isEmpty()) {
                        appendLuaOutput("Enter the player UID first (run state() to list players).");
                        return;
                    }
                    executeLuaViaDll("native.killPlayer(" + idStr + ")");
                });
                row->addWidget(killBtn);
                layout->insertWidget(layout->count() - 1, widget);
            }
            layout->insertWidget(layout->count() - 1,
                nativeCmdRow("State / List", "Dump command status and player UIDs to console",
                    {{"state()", "native.state()"}}));
        } else if (tab == 2) { // Teleport tab
            // Position inputs
            if (!inputIds.isEmpty() && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : inputIds) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createInputRow(c));
            }

            // Random Teleport toggle
            for (int id : toggleIds) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createToggleRow(c, c->subtitle));
            }

            // Teleport section: saved positions manager
            if (headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }

            // Save position UI
            auto* saveCard = new QWidget();
            saveCard->setObjectName("cheatCard");
            saveCard->setMinimumHeight(52);
            auto* saveLayout = new QHBoxLayout(saveCard);
            saveLayout->setContentsMargins(16, 10, 16, 10);
            saveLayout->setSpacing(8);

            m_tpNameInput = new QLineEdit();
            m_tpNameInput->setObjectName("valueInput");
            m_tpNameInput->setPlaceholderText(QString::fromUtf8(lang(m_currentLanguage).posPlaceholder));
            m_tpNameInput->setFixedWidth(160);
            saveLayout->addWidget(m_tpNameInput, 1);

            auto* saveBtn = new QPushButton(QString::fromUtf8(lang(m_currentLanguage).save));
            saveBtn->setObjectName("actionButton");
            saveBtn->setFixedWidth(60);
            connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveTeleportClicked);
            saveLayout->addWidget(saveBtn);

            layout->insertWidget(layout->count() - 1, saveCard);

            // Saved positions list
            m_tpListWidget = new QWidget();
            m_tpListLayout = new QVBoxLayout(m_tpListWidget);
            m_tpListLayout->setContentsMargins(0, 0, 0, 0);
            m_tpListLayout->setSpacing(4);
            layout->insertWidget(layout->count() - 1, m_tpListWidget);

            loadSavedTeleports();
        } else if (tab == 6) { // Vision tab
            if (!toggleIds.isEmpty() && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : toggleIds) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createToggleRow(c, c->subtitle));
            }
            // Zelvex Native Vision (DLL)
            {
                auto nativeCmdRow = [this](const QString& title, const QString& sub,
                                          const QList<QPair<QString, QString>>& actions) -> QWidget* {
                    auto* widget = new QWidget();
                    widget->setObjectName("cheatCard");
                    widget->setMinimumHeight(52);
                    auto* row = new QHBoxLayout(widget);
                    row->setContentsMargins(16, 10, 16, 10);
                    row->setSpacing(8);
                    auto* textW = new QWidget();
                    auto* textL = new QVBoxLayout(textW);
                    textL->setContentsMargins(0, 0, 0, 0);
                    textL->setSpacing(2);
                    auto* nameL = new QLabel(title);
                    nameL->setObjectName("toggleName");
                    textL->addWidget(nameL);
                    if (!sub.isEmpty()) {
                        auto* subL = new QLabel(sub);
                        subL->setObjectName("toggleSubtitle");
                        textL->addWidget(subL);
                    }
                    row->addWidget(textW, 1);
                    for (const auto& a : actions) {
                        auto* b = new QPushButton(a.first);
                        b->setObjectName("ghostButton");
                        b->setFixedHeight(28);
                        QString cmd = a.second;
                        connect(b, &QPushButton::clicked, this, [this, cmd]() {
                            executeLuaViaDll(cmd);
                        });
                        row->addWidget(b);
                    }
                    return widget;
                };
                if (headerIdx < tabDefs[tab].sectionHeaders.size())
                    layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
                layout->insertWidget(layout->count() - 1,
                    nativeCmdRow("Hit Walls", "Hit players through walls (attack through obstacles)",
                        {{"ON", "native.hitWalls(1)"}, {"OFF", "native.hitWalls(0)"}}));
                layout->insertWidget(layout->count() - 1,
                    nativeCmdRow("Ground See", "See through ground terrain surfaces",
                        {{"ON", "native.groundSee(1)"}, {"OFF", "native.groundSee(0)"}}));
                layout->insertWidget(layout->count() - 1,
                    nativeCmdRow("Air See", "Remove invisible air walls",
                        {{"ON", "native.airSee(1)"}, {"OFF", "native.airSee(0)"}}));
            }
            m_itemsTabLoaded = false;
        } else if (tab == 7) { // Items tab
            loadItemsTab();
            m_tabLoaded[tab] = true;
            return;
        } else if (tab == 8) { // Lua tab
            loadLuaTab();
            m_tabLoaded[tab] = true;
            return;
        } else {
            // General tab attach button (top of General tab)
            if (tab == 0) {
                // Language selector
                auto* langCard = new QWidget();
                langCard->setObjectName("cheatCard");
                langCard->setMinimumHeight(52);
                auto* langLayout = new QHBoxLayout(langCard);
                langLayout->setContentsMargins(16, 10, 16, 10);
                langLayout->setSpacing(6);

                auto* langLabel = new QLabel(QString::fromUtf8(trUI("lang_label")));
                langLabel->setStyleSheet("font-size: 12px; font-weight: 700; color: #e6e9ee; background: transparent; border: none;");
                langLayout->addWidget(langLabel);
                langLayout->addStretch();

                Language allLangs[] = {Language::English, Language::Italiano, Language::Chinese, Language::Espanol, Language::Russian, Language::Turkish, Language::Vietnamese};
                for (int i = 0; i < 7; i++) {
                    auto* btn = new QPushButton(languageName(allLangs[i]));
                    btn->setObjectName("langBtn");
                    btn->setCheckable(true);
                    btn->setChecked(allLangs[i] == m_currentLanguage);
                    btn->setFixedHeight(30);
                    btn->setProperty("lang", static_cast<int>(allLangs[i]));
                    connect(btn, &QPushButton::clicked, this, [this, btn]() {
                        setLanguage(static_cast<Language>(btn->property("lang").toInt()));
                    });
                    m_langButtons[i] = btn;
                    langLayout->addWidget(btn);
                }
                layout->insertWidget(layout->count() - 1, langCard);

                m_generalAttachBtn = new QPushButton();
                m_generalAttachBtn->setFixedHeight(42);
                m_generalAttachBtn->setCursor(Qt::PointingHandCursor);
                connect(m_generalAttachBtn, &QPushButton::clicked, this, &MainWindow::onAttachClicked);
                layout->insertWidget(layout->count() - 1, m_generalAttachBtn);
                layout->insertSpacing(layout->count() - 1, 8);
                updateGeneralAttachBtn();

                // ── Global hotkeys (Ctrl+Alt+1/2/3) ──
                registerHotkeys();
                QSettings settings("Zelvex", "Zelvex");
                auto* hkCard = new QWidget();
                hkCard->setObjectName("cheatCard");
                auto* hkLayout = new QVBoxLayout(hkCard);
                hkLayout->setContentsMargins(16, 12, 16, 12);
                hkLayout->setSpacing(6);
                auto* hkTitle = new QLabel("GLOBAL HOTKEYS  -  Lua one-liners, work while the game is focused");
                hkTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #e6e9ee; background: transparent; border: none;");
                hkLayout->addWidget(hkTitle);
                for (int i = 0; i < 3; i++) {
                    auto* row = new QWidget();
                    auto* rl = new QHBoxLayout(row);
                    rl->setContentsMargins(0, 0, 0, 0);
                    rl->setSpacing(8);
                    auto* keyLbl = new QLabel(QString("Ctrl+Alt+%1").arg(i + 1));
                    keyLbl->setStyleSheet("font-size: 11px; font-weight: 700; color: #5AA3FF; background: transparent; border: none;");
                    keyLbl->setFixedWidth(70);
                    rl->addWidget(keyLbl);
                    auto* edit = new QLineEdit(settings.value(QString("hotkey/slot%1").arg(i)).toString());
                    edit->setObjectName("valueInput");
                    edit->setPlaceholderText("e.g. Player:noclip(true)   /   Actor:killAura(1)");
                    edit->setFixedHeight(28);
                    connect(edit, &QLineEdit::textChanged, this, [i](const QString& t) {
                        QSettings s("Zelvex", "Zelvex");
                        s.setValue(QString("hotkey/slot%1").arg(i), t);
                    });
                    m_hotkeyEdits[i] = edit;
                    rl->addWidget(edit, 1);
                    hkLayout->addWidget(row);
                }
                layout->insertWidget(layout->count() - 1, hkCard);

                // ── Pattern Doctor ──
                auto* docCard = new QWidget();
                docCard->setObjectName("cheatCard");
                docCard->setMinimumHeight(52);
                auto* docLayout = new QHBoxLayout(docCard);
                docLayout->setContentsMargins(16, 10, 16, 10);
                docLayout->setSpacing(8);
                auto* docText = new QWidget();
                auto* docTextL = new QVBoxLayout(docText);
                docTextL->setContentsMargins(0, 0, 0, 0);
                docTextL->setSpacing(2);
                auto* docName = new QLabel("Pattern Doctor");
                docName->setObjectName("toggleName");
                docTextL->addWidget(docName);
                auto* docSub = new QLabel("Check every subsystem on this game build (run after game updates)");
                docSub->setObjectName("toggleSubtitle");
                docTextL->addWidget(docSub);
                docLayout->addWidget(docText, 1);
                auto* docBtn = new QPushButton("RUN CHECK");
                docBtn->setObjectName("ghostButton");
                docBtn->setFixedHeight(28);
                connect(docBtn, &QPushButton::clicked, this, [this]() {
                    switchTab(8);
                    executeLuaViaDll(
                        "local s = native.state() or \"\"\n"
                        "if s == \"\" then print(\"state() failed - inject the DLL first\") return end\n"
                        "for tok in s:gmatch(\"%S+\") do\n"
                        "    local k, v = tok:match(\"^(%w+)=(.+)$\")\n"
                        "    if k then\n"
                        "        local good = (v == \"ok\") or (v == \"1\")\n"
                        "        local bad  = (v == \"missing\") or (v == \"no\") or (v == \"0\")\n"
                        "        local mark = good and \"[OK]  \" or (bad and \"[BAD] \" or \"[??]  \")\n"
                        "        print(mark .. k .. \" = \" .. v)\n"
                        "    end\n"
                        "end\n"
                        "print(\"---\")\n"
                        "print(\"players visible: \" .. tostring(Players.count()))\n");
                });
                docLayout->addWidget(docBtn);
                layout->insertWidget(layout->count() - 1, docCard);
            }

            // Default rendering for other tabs
            if (!toggleIds.isEmpty() && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : toggleIds) {
                const CheatDef* c = findCheatById(id);
                if (!c) continue;
                if (isDisabledCheat(c->id)) {
                    layout->insertWidget(layout->count() - 1, createDisabledRow(c));
                } else {
                    layout->insertWidget(layout->count() - 1, createToggleRow(c, c->subtitle));
                }
            }

            if (tab == 0) {
                layout->insertWidget(layout->count() - 1, createPageHeader("CREDITS"));

                auto* creditsCard = new QWidget();
                creditsCard->setObjectName("cheatCard");
                auto* creditsLayout = new QVBoxLayout(creditsCard);
                creditsLayout->setContentsMargins(16, 12, 16, 12);
                creditsLayout->setSpacing(5);

                auto* creditsTitle = new QLabel("Zelvex Executor v4.0");
                creditsTitle->setStyleSheet("font-size: 13px; font-weight: 800; color: #ffffff; background: transparent; border: none;");
                creditsLayout->addWidget(creditsTitle);

                const char* creditsLines[] = {
                    "Made with <3 by nyxdev_",
                    "Item library: Mini World CREATA Wiki (wiki.miniworldgame.com)",
                    "Game: Mini World: Block Art  - all trademarks belong to their owners",
                    "Use at your own risk  - for educational purposes only"
                };
                for (const char* line : creditsLines) {
                    auto* creditLine = new QLabel(QString::fromUtf8(line));
                    creditLine->setStyleSheet("font-size: 11px; color: #8b93a3; background: transparent; border: none;");
                    creditLine->setWordWrap(true);
                    creditsLayout->addWidget(creditLine);
                }
                layout->insertWidget(layout->count() - 1, creditsCard);
            }

            if (!actionIds.isEmpty() && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : actionIds) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createActionRow(c));
            }

            if (!inputIds.isEmpty() && headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
            }
            for (int id : inputIds) {
                const CheatDef* c = findCheatById(id);
                if (c) layout->insertWidget(layout->count() - 1, createInputRow(c));
            }

            for (int id : comboIds) {
                const CheatDef* c = findCheatById(id);
                if (!c) continue;
                if (c->type == CheatType::COMBO_INT && c->comboItems) {
                    if (headerIdx < tabDefs[tab].sectionHeaders.size()) {
                layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
                    }
                    m_conditionCombo = new QComboBox();
                    m_conditionCombo->setObjectName("toolCombo");
                    QStringList items = QString(c->comboItems).split(',', Qt::SkipEmptyParts);
                    for (const QString& item : items) {
                        int colonIdx = item.indexOf(':');
                        if (colonIdx >= 0)
                            m_conditionCombo->addItem(item.mid(colonIdx + 1).trimmed(), item.left(colonIdx).trimmed());
                        else
                            m_conditionCombo->addItem(item.trimmed());
                    }
                    const CheatDef* comboCheat = c;
                    connect(m_conditionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, comboCheat](int) {
                        if (!m_mm.isAttached()) return;
                        quint64 base = m_mm.parseAddress(comboCheat->address);
                        if (base == 0) return;
                        QList<qint64> offsets = parseCheatOffsets(comboCheat->offsetStr);
                        quint64 addr = m_mm.resolvePointer(base, offsets);
                        if (addr == 0) return;
                        QString valStr = m_conditionCombo->currentData().toString();
                        m_mm.writeUInt(addr, valStr.toUInt());
                    });
                    layout->insertWidget(layout->count() - 1, createComboRow("Player Condition Mode", m_conditionCombo));
                }
            }

            // ── Native DLL sections per tab ──
            {
                auto nativeCmdRow = [this](const QString& title, const QString& sub,
                                          const QList<QPair<QString, QString>>& actions) -> QWidget* {
                    auto* widget = new QWidget();
                    widget->setObjectName("cheatCard");
                    widget->setMinimumHeight(52);
                    auto* row = new QHBoxLayout(widget);
                    row->setContentsMargins(16, 10, 16, 10);
                    row->setSpacing(8);
                    auto* textW = new QWidget();
                    auto* textL = new QVBoxLayout(textW);
                    textL->setContentsMargins(0, 0, 0, 0);
                    textL->setSpacing(2);
                    auto* nameL = new QLabel(title);
                    nameL->setObjectName("toggleName");
                    textL->addWidget(nameL);
                    if (!sub.isEmpty()) {
                        auto* subL = new QLabel(sub);
                        subL->setObjectName("toggleSubtitle");
                        textL->addWidget(subL);
                    }
                    row->addWidget(textW, 1);
                    for (const auto& a : actions) {
                        auto* b = new QPushButton(a.first);
                        b->setObjectName("ghostButton");
                        b->setFixedHeight(28);
                        QString cmd = a.second;
                        connect(b, &QPushButton::clicked, this, [this, cmd]() {
                            executeLuaViaDll(cmd);
                        });
                        row->addWidget(b);
                    }
                    return widget;
                };
                auto nativeInputRow = [this](const QString& title, const QString& sub,
                                            const QString& placeholder, const QString& btnLabel,
                                            const QString& cmdTemplate) -> QWidget* {
                    auto* widget = new QWidget();
                    widget->setObjectName("cheatCard");
                    widget->setMinimumHeight(52);
                    auto* row = new QHBoxLayout(widget);
                    row->setContentsMargins(16, 10, 16, 10);
                    row->setSpacing(8);
                    auto* textW = new QWidget();
                    auto* textL = new QVBoxLayout(textW);
                    textL->setContentsMargins(0, 0, 0, 0);
                    textL->setSpacing(2);
                    auto* nameL = new QLabel(title);
                    nameL->setObjectName("toggleName");
                    textL->addWidget(nameL);
                    if (!sub.isEmpty()) {
                        auto* subL = new QLabel(sub);
                        subL->setObjectName("toggleSubtitle");
                        textL->addWidget(subL);
                    }
                    row->addWidget(textW, 1);
                    auto* edit = new QLineEdit();
                    edit->setObjectName("toolCombo");
                    edit->setPlaceholderText(placeholder);
                    edit->setFixedHeight(28);
                    edit->setFixedWidth(130);
                    row->addWidget(edit);
                    auto* btn = new QPushButton(btnLabel);
                    btn->setObjectName("ghostButton");
                    btn->setFixedHeight(28);
                    connect(btn, &QPushButton::clicked, this, [this, edit, cmdTemplate]() {
                        QString val = edit->text().trimmed();
                        if (val.isEmpty()) return;
                        executeLuaViaDll(cmdTemplate.arg(val));
                    });
                    row->addWidget(btn);
                    return widget;
                };

                if (tab == 0) {
                    // Misc tab — Zelvex Native World
                    if (headerIdx < tabDefs[tab].sectionHeaders.size())
                        layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
                    layout->insertWidget(layout->count() - 1,
                        nativeCmdRow("Room Owner", "Check who owns the current room",
                            {{"CHECK", "native.roomOwner()"}}));
                    layout->insertWidget(layout->count() - 1,
                        nativeCmdRow("Room Map", "Check the current room/map id",
                            {{"CHECK", "native.roomMap()"}}));
                    layout->insertWidget(layout->count() - 1,
                        nativeInputRow("Chat Message", "Send a chat message to all players",
                            "message...", "SEND", "native.chat('%1')"));
                    layout->insertWidget(layout->count() - 1,
                        nativeInputRow("Set Time", "Set the world time (integer ticks)",
                            "time value", "SET", "native.setTime(%1)"));
                    layout->insertWidget(layout->count() - 1,
                        nativeInputRow("Time Speed", "Set the day/night cycle speed multiplier",
                            "speed (1=normal)", "SET", "native.setTimespeed(%1)"));
                } else if (tab == 1) {
                    // Player tab — Zelvex Native Player
                    if (headerIdx < tabDefs[tab].sectionHeaders.size())
                        layout->insertWidget(layout->count() - 1, createPageHeader(trSection(tabDefs[tab].sectionHeaders[headerIdx++])));
                    layout->insertWidget(layout->count() - 1,
                        nativeCmdRow("Revive", "Resurrect yourself after death",
                            {{"REVIVE", "native.revive()"}}));
                    layout->insertWidget(layout->count() - 1,
                        nativeCmdRow("No Drop", "Prevent dropping items from inventory",
                            {{"ON", "native.noDrop(1)"}, {"OFF", "native.noDrop(0)"}}));
                    layout->insertWidget(layout->count() - 1,
                        nativeCmdRow("Jump Fly", "Fly when you jump",
                            {{"ON", "native.jumpFly(1)"}, {"OFF", "native.jumpFly(0)"}}));
                    layout->insertWidget(layout->count() - 1,
                        nativeCmdRow("Slow Fall", "Reduce fall speed dramatically",
                            {{"ON", "native.slowFall(1)"}, {"OFF", "native.slowFall(0)"}}));
                    layout->insertWidget(layout->count() - 1,
                        nativeInputRow("Set Jump Height", "Change how high you jump (default 1.0)",
                            "height", "SET", "native.setJump(%1)"));
                    layout->insertWidget(layout->count() - 1,
                        nativeInputRow("Set Scale", "Change player model size (default 1.0)",
                            "scale", "SET", "native.setScale(%1)"));
                }
            }
        }
        m_tabLoaded[tab] = true;
    }

    if (firstInit)
        switchTab(8);
}

// ── Tab Management ──

void MainWindow::switchTab(int index) {
    if (index < 0 || index >= m_tabIcons.size()) return;

    // Lazy-build once per tab; revisit only re-syncs toggle states.
    // (Previously every click cleared+rebuilt the page, which destroyed
    // the custom Lua/Items pages and left them blank on the second visit.)
    if (!m_tabLoaded[index]) {
        m_tabLoaded[index] = true;
        loadEntries(index);
    } else {
        syncTabStates(index);
    }

    setTabIconStates(index);
    m_stack->setCurrentIndex(index);
    m_activeTab = index;
    animatePageChange(index);
}

void MainWindow::onTabClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    switchTab(btn->property("tabIndex").toInt());
}

void MainWindow::updateTabIcon(int index) {
    auto tabDefs = UILayout::getTabDefs();
    if (index < 0 || index >= m_tabIcons.size()) return;
    if (tabDefs[index].logo) return;
    bool active = (index == m_activeTab);
    bool hover = m_tabIcons[index]->property("hover").toBool();
    m_tabIcons[index]->setPixmap(AppIcons::labelPixmap(tabDefs[index].icon,
        active ? QColor("#5AA3FF") : (hover ? QColor("#E6E9EE") : QColor("#9AA3B2")), 24));
}

void MainWindow::setTabIconStates(int index) {
    auto tabDefs = UILayout::getTabDefs();
    for (int i = 0; i < m_tabIcons.size() && i < tabDefs.size(); i++) {
        bool active = (i == index);
        auto* container = m_tabIcons[i]->parentWidget();
        if (container) {
            container->setProperty("active", active);
            container->style()->unpolish(container);
            container->style()->polish(container);
        }
        if (i < m_tabLabels.size()) {
            m_tabLabels[i]->setProperty("active", active);
            m_tabLabels[i]->style()->unpolish(m_tabLabels[i]);
            m_tabLabels[i]->style()->polish(m_tabLabels[i]);
        }
        m_tabIcons[i]->setProperty("active", active);
        m_tabIcons[i]->style()->unpolish(m_tabIcons[i]);
        m_tabIcons[i]->style()->polish(m_tabIcons[i]);
        m_tabIcons[i]->setProperty("hover", false);
        updateTabIcon(i);
    }
}

void MainWindow::animatePageChange(int index) {
    QWidget* page = m_stack->widget(index);
    if (!page) return;
    if (!m_pageEffects.contains(index)) {
        auto* effect = new QGraphicsOpacityEffect(page);
        effect->setOpacity(1.0);
        page->setGraphicsEffect(effect);
        m_pageEffects[index] = effect;
    }
    QGraphicsOpacityEffect* effect = m_pageEffects.value(index);
    double from = (effect->opacity() >= 0.99) ? 0.0 : effect->opacity();
    auto* anim = new QPropertyAnimation(effect, "opacity", page);
    anim->setDuration(150);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(from);
    anim->setEndValue(1.0);
    connect(anim, &QPropertyAnimation::finished, anim, &QObject::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::syncTabStates(int tab) {
    if (!m_mm.isAttached()) return;
    auto tabDefs = UILayout::getTabDefs();
    if (tab < 0 || tab >= tabDefs.size()) return;

    QSet<int> ids;
    for (int id : tabDefs[tab].cheatIds) ids.insert(id);

    for (const ToggleItem& ti : m_toggles) {
        if (!ti.cheat || !ti.pill || !ids.contains(ti.cheat->id)) continue;
        const CheatDef& c = *ti.cheat;
        if (c.type != CheatType::TOGGLE_AOB_JMP && c.type != CheatType::TOGGLE_AOB_DB)
            continue;

        quint64 addr = 0;
        if (c.symbolName && c.module) {
            quint64 sym = m_mm.resolvePdbSymbol(c.module, c.symbolName);
            if (sym) addr = sym + c.symbolOffset;
        }
        if (!addr && c.aob && c.module) {
            QByteArray pattern, mask;
            pattern = parseAobPattern(c.aob, mask);
            addr = m_mm.aobScanSingle(c.module, pattern, mask);
        }
        if (!addr) continue;

        quint64 patchAddr = addr + c.patchOffset;
        QByteArray cur = m_mm.readBytes(patchAddr, 6);
        bool active = false;
        if (c.type == CheatType::TOGGLE_AOB_JMP) {
            active = !cur.isEmpty() && static_cast<quint8>(cur[0]) == 0xE9;
        } else if (c.enableBytes) {
            QStringList parts = QString(c.enableBytes).split(' ', Qt::SkipEmptyParts);
            QByteArray exp;
            for (const QString& p : parts) exp.append(static_cast<char>(p.toUInt(nullptr, 16)));
            active = !exp.isEmpty() && cur.left(exp.size()) == exp;
        }
        if (ti.pill->isChecked() != active) ti.pill->setChecked(active);
    }
}

void MainWindow::appendConsoleHtml(const QString& html, const QString& textPlain) {
    if (!m_luaConsole) return;
    m_luaConsole->append(html);
    if (m_consoleAutoScroll) {
        QScrollBar* sb = m_luaConsole->verticalScrollBar();
        if (sb) sb->setValue(sb->maximum());
    }
    Q_UNUSED(textPlain);
}

void MainWindow::updateStatusVisual(bool attached) {
    if (m_statusDot) {
        m_statusDot->setStyleSheet(QString("background-color: %1; border-radius: 4px; border: none;")
                                       .arg(attached ? "#34D399" : "#3a4350"));
    }
    if (m_statusLabel) {
        m_statusLabel->setProperty("attached", attached);
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
    }
    if (m_headerPidLabel) {
        if (attached) {
            m_headerPidLabel->setText(QString("GAME ATTACHED - PID %1").arg(m_mm.processId()));
            m_headerPidLabel->setToolTip("Attached to the game process  - native.* commands are live");
            m_headerPidLabel->setProperty("pidOn", true);
        } else {
            m_headerPidLabel->setText("GAME OFFLINE");
            m_headerPidLabel->setToolTip("Not attached to any game process  - click ATTACH to begin");
            m_headerPidLabel->setProperty("pidOn", false);
        }
        m_headerPidLabel->style()->unpolish(m_headerPidLabel);
        m_headerPidLabel->style()->polish(m_headerPidLabel);
    }
}

void MainWindow::updateSidebarBody(bool expanded) {
    int iconWidth = expanded ? 28 : (SIDEBAR_COLLAPSED - 32);
    for (auto* iconLabel : m_tabIcons) {
        iconLabel->setFixedWidth(iconWidth);
        if (auto* textLabel = iconLabel->parentWidget()->findChild<QLabel*>("tabText")) {
            textLabel->setVisible(expanded);
        }
    }
    m_sidebarHeader->setVisible(expanded);
    m_sidebarSub->setVisible(expanded);
    m_statusDot->setVisible(expanded);
    m_statusLabel->setVisible(expanded);
    m_attachBtn->setVisible(expanded);
}

// ── Sidebar Toggle ──

void MainWindow::toggleSidebar() {
    if (m_sidebarAnim) {
        m_sidebarAnim->stop();
        delete m_sidebarAnim;
        m_sidebarAnim = nullptr;
    }

    m_sidebarCollapsed = !m_sidebarCollapsed;
    int newWidth = m_sidebarCollapsed ? SIDEBAR_COLLAPSED : SIDEBAR_EXPANDED;

    m_sidebarAnim = new QParallelAnimationGroup(this);
    connect(m_sidebarAnim, &QParallelAnimationGroup::finished, this, [this]() {
        if (m_sidebarAnim) {
            m_sidebarAnim->deleteLater();
            m_sidebarAnim = nullptr;
        }
    });

    auto* animMax = new QPropertyAnimation(m_sidebar, "maximumWidth");
    animMax->setDuration(200);
    animMax->setEasingCurve(QEasingCurve::InOutCubic);
    animMax->setStartValue(m_sidebar->maximumWidth());
    animMax->setEndValue(newWidth);

    auto* animMin = new QPropertyAnimation(m_sidebar, "minimumWidth");
    animMin->setDuration(200);
    animMin->setEasingCurve(QEasingCurve::InOutCubic);
    animMin->setStartValue(m_sidebar->minimumWidth());
    animMin->setEndValue(newWidth);

    m_sidebarAnim->addAnimation(animMax);
    m_sidebarAnim->addAnimation(animMin);
    m_sidebarAnim->start();

    updateSidebarBody(!m_sidebarCollapsed);
}

// ── Loading Screen Stages ──

void MainWindow::initLoadingStages() {
    if (!m_loadingScreen) return;

    m_loadingScreen->setStatus("Building interface...");
    m_loadingScreen->setProgress(10);

    QTimer::singleShot(100, this, [this]() {
        loadEntries();
        m_loadingScreen->setProgress(40);
        m_loadingScreen->setStatus("Scanning...");

        QTimer::singleShot(100, this, [this]() {
            bool gameRunning = MemoryManager::isProcessRunning("MiniGameApp.exe");
            if (gameRunning) {
                detectStalePatches();
                preScanAobs();
                m_loadingScreen->setProgress(70);
                m_loadingScreen->setStatus("Attaching...");

                QTimer::singleShot(100, this, [this]() {
                    onAttachClicked(true);
                    m_healthTimer->start();
                    m_loadingScreen->setProgress(100);
                    m_loadingScreen->setStatus("Ready!");

                    QTimer::singleShot(200, this, [this]() {
                        m_loadingScreen->close();
                        delete m_loadingScreen;
                        m_loadingScreen = nullptr;
                    });
                });
            } else {
                m_healthTimer->start();
                m_loadingScreen->setProgress(100);
                m_loadingScreen->setStatus("Ready!");

                QTimer::singleShot(200, this, [this]() {
                    m_loadingScreen->close();
                    delete m_loadingScreen;
                    m_loadingScreen = nullptr;
                });
            }
        });
    });
}

// ── Attach/Detach ──

void MainWindow::onAttachClicked(bool isStartup) {
    if (m_mm.isAttached()) {
        m_refreshTimer->stop();
        stopRandomTeleport();
        for (auto it = m_injections.begin(); it != m_injections.end(); ++it) {
            if (it->remoteThread) { TerminateThread(it->remoteThread, 0); CloseHandle(it->remoteThread); it->remoteThread = nullptr; }
        }
        Sleep(100);
        m_mm.restoreAllPatches();
        m_injections.clear();
        m_mm.detach();
        m_gameWindow = nullptr;
        m_resolvedAddrCache.clear();
        m_resolveFailCount.clear();
        const auto& L = lang(m_currentLanguage);
        m_attachBtn->setText(QString::fromUtf8(L.attachBtn));
        m_statusLabel->setText("GAME OFFLINE");
        updateStatusVisual(false);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        updateGeneralAttachBtn();
        refreshLuaAttachState();
        for (const ToggleItem& ti : m_toggles) {
            if (ti.pill) ti.pill->setChecked(false);
        }
        return;
    }

    if (!m_mm.attach("MiniGameApp.exe")) {
        const auto& L = lang(m_currentLanguage);
        m_statusLabel->setText(QString::fromUtf8(L.gameNotFound));
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        updateStatusVisual(false);
        return;
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(m_mm.handle(), &exitCode) || exitCode != STILL_ACTIVE) {
        m_mm.detach();
        const auto& L = lang(m_currentLanguage);
        m_statusLabel->setText(QString::fromUtf8(L.invalidHandle));
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        updateStatusVisual(false);
        return;
    }

    m_gameWindow = m_mm.findMainWindow(m_mm.processId());
    const auto& L = lang(m_currentLanguage);
    m_attachBtn->setText(QString("%1 [%2]").arg(QString::fromUtf8(L.detachBtn)).arg(m_mm.processId()));
    m_statusLabel->setText("GAME ATTACHED");
    updateStatusVisual(true);
    m_statusLabel->setVisible(true);
    m_statusDot->setVisible(true);
    updateGeneralAttachBtn();
    refreshLuaAttachState();
    m_refreshTimer->start();

    if (!isStartup) {
        // Defer heavy scanning when user clicks Attach manually
        QTimer::singleShot(0, this, [this]() {
            detectStalePatches();
            preScanAobs();
        });
    }
}

// ── Value Refresh Timer ──

void MainWindow::refreshTimer() {
    if (!m_mm.isAttached()) return;

    for (const InputItem& item : m_inputs) {
        if (!item.cheat || !item.input) continue;
        if (item.input->hasFocus()) continue;
        if (!item.cheat->address) continue;

        int cheatId = item.cheat->id;

        // Skip entries that failed to resolve 5+ times in a row
        if (m_resolveFailCount.value(cheatId, 0) >= 5) continue;

        quint64 addr = 0;
        if (m_resolvedAddrCache.contains(cheatId)) {
            addr = m_resolvedAddrCache[cheatId];
        } else {
            quint64 base = m_mm.parseAddress(item.cheat->address);
            if (base == 0) { m_resolveFailCount[cheatId]++; continue; }
            QList<qint64> offsets = parseCheatOffsets(item.cheat->offsetStr);
            addr = m_mm.resolvePointer(base, offsets);
            if (addr == 0) { m_resolveFailCount[cheatId]++; continue; }
            m_resolvedAddrCache[cheatId] = addr;
        }

        m_resolveFailCount[cheatId] = 0;

        QString vt = item.cheat->valueType;
        QString newText;
        if (vt == "Float") {
            float val = m_mm.readFloat(addr);
            newText = QString::number(val, 'f', 2);
        } else if (vt == "Double") {
            double val = m_mm.readDouble(addr);
            newText = QString::number(val, 'f', 4);
        } else if (vt == "4 Bytes") {
            newText = QString::number(m_mm.readUInt(addr));
        } else if (vt == "2 Bytes") {
            newText = QString::number(static_cast<quint16>(m_mm.readShort(addr)));
        } else if (vt == "1 Byte") {
            newText = QString::number(m_mm.readByte(addr));
        }
        if (!newText.isEmpty() && item.input->text() != newText) {
            item.input->setText(newText);
        }
    }

    // Re-apply game-overwritten patches every tick
    for (auto it = m_injections.begin(); it != m_injections.end(); ++it) {
        if (it->patchAddress == 0 || it->expectedBytes.isEmpty()) continue;
        QByteArray current = m_mm.readBytes(it->patchAddress, it->expectedBytes.size());
        if (current.size() == it->expectedBytes.size() && current != it->expectedBytes) {
            m_mm.writeBytes(it->patchAddress, it->expectedBytes);
        }
    }
}

// ── Write Pointer Value ──

bool MainWindow::writePointerValue(const CheatDef& cheat, const QString& valueStr) {
    if (!m_mm.isAttached() || !cheat.address) return false;
    quint64 base = m_mm.parseAddress(cheat.address);
    if (base == 0) return false;
    QList<qint64> offsets = parseCheatOffsets(cheat.offsetStr);
    quint64 addr = m_mm.resolvePointer(base, offsets);
    if (addr == 0) return false;

    bool ok;
    QString vt = cheat.valueType;
    if (vt == "Float") {
        float val = valueStr.toFloat(&ok);
        if (ok) return m_mm.writeFloat(addr, val);
    } else if (vt == "Double") {
        double val = valueStr.toDouble(&ok);
        if (ok) return m_mm.writeDouble(addr, val);
    } else if (vt == "4 Bytes") {
        quint32 val = valueStr.toUInt(&ok);
        if (ok) return m_mm.writeUInt(addr, val);
    } else if (vt == "2 Bytes") {
        quint16 val = valueStr.toUShort(&ok);
        if (ok) return m_mm.writeShort(addr, val);
    } else if (vt == "1 Byte") {
        quint8 val = static_cast<quint8>(valueStr.toUShort(&ok));
        if (ok) return m_mm.writeByte(addr, val);
    }
    return false;
}

// ── AOB Parsing ──

QByteArray MainWindow::parseAobPattern(const QString& patternStr, QByteArray& mask) const {
    QByteArray pattern;
    mask.clear();
    QStringList parts = patternStr.trimmed().split(' ', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        if (p == "?" || p == "??") {
            pattern.append('\x00');
            mask.append('\x00');
        } else {
            pattern.append(static_cast<char>(p.toUInt(nullptr, 16)));
            mask.append('\xff');
        }
    }
    return pattern;
}

// ── Script Routing ──

void MainWindow::applyCheat(const CheatDef& cheat) {
    if (!m_mm.isAttached()) return;
    if (isDisabledCheat(cheat.id)) return;

    if (cheat.id == 1807617779) {
        if (!m_mm.getSharedMemory() && !injectLuaDll()) {
            appendLuaOutput("[crash-host] DLL injection failed - attach and retry");
            return;
        }
        appendLuaOutput("[crash-host] spamming item 1105 x500 via native.batch...");
        executeLuaViaDll("native.giveItemBatch(1105,1,500)");
        return;
    }

    switch (cheat.type) {
        case CheatType::TOGGLE_AOB_DB:
            applyAobDbPatch(cheat);
            break;
        case CheatType::TOGGLE_AOB_JMP:
            applyAobJmpInject(cheat);
            break;
        case CheatType::TOGGLE_THREAD:
            applyTerrainEditor(cheat);
            break;
        case CheatType::TOGGLE_RANDOM_TP:
            startRandomTeleport();
            break;
        case CheatType::ACTION_WIN32:
            applyReadme();
            break;
        case CheatType::ACTION_AOB_DB:
            applyAobDbAction(cheat);
            break;
        case CheatType::ACTION_AOB_JMP:
            applyAobJmpAction(cheat);
            break;
        case CheatType::ACTION_GIVE_ITEM:
            applyGiveItem();
            break;
        default:
            break;
    }
}

void MainWindow::restoreCheat(const CheatDef& cheat) {
    if (!m_mm.isAttached()) return;

    if (cheat.type == CheatType::TOGGLE_RANDOM_TP) {
        stopRandomTeleport();
        return;
    }

    auto it = m_injections.find(cheat.id);
    if (it == m_injections.end()) return;

    InjectionRecord rec = it.value();

    if (rec.remoteThread) {
        if (cheat.type == CheatType::TOGGLE_THREAD) {
            TerminateThread(rec.remoteThread, 0);
            Sleep(100);
        } else {
            WaitForSingleObject(rec.remoteThread, 2000);
        }
        CloseHandle(rec.remoteThread);
    }

    if (rec.remoteCodeAddress)
        m_mm.freeRemote(rec.remoteCodeAddress);
    if (rec.remoteDataAddress)
        m_mm.freeRemote(rec.remoteDataAddress);
    if (rec.remoteCmdAddress)
        m_mm.freeRemote(rec.remoteCmdAddress);

    if (rec.patchAddress && rec.patchSize > 0)
        m_mm.restorePatch(rec.patchAddress);

    m_injections.erase(it);
    m_statusLabel->setText(QString("%1 disabled").arg(cheat.name));
    m_statusLabel->setVisible(true);
    m_statusDot->setVisible(true);
}

// ── AOB + DB Patch ──

void MainWindow::applyAobDbPatch(const CheatDef& cheat) {
    quint64 patchAddr = 0;

    if (cheat.symbolName && cheat.module) {
        quint64 symAddr = m_mm.resolvePdbSymbol(cheat.module, cheat.symbolName);
        if (symAddr == 0) {
            m_statusLabel->setText(QString("Symbol not found: %1").arg(cheat.symbolName));
            return;
        }
        patchAddr = symAddr + cheat.symbolOffset;
    } else if (cheat.aob && cheat.module) {
        QByteArray pattern, mask;
        pattern = parseAobPattern(cheat.aob, mask);
        quint64 foundAddr = m_mm.aobScanSingle(cheat.module, pattern, mask);
        if (foundAddr == 0) {
            m_statusLabel->setText(QString("AOB scan failed: %1").arg(cheat.name));
            return;
        }
        patchAddr = foundAddr + cheat.patchOffset;
    } else {
        m_statusLabel->setText(QString("No AOB or symbol for: %1").arg(cheat.name));
        return;
    }

    QByteArray patchBytes;
    QStringList bytes = QString(cheat.enableBytes).split(' ', Qt::SkipEmptyParts);
    for (const QString& b : bytes)
        patchBytes.append(static_cast<char>(b.toUInt(nullptr, 16)));

    if (m_mm.backupAndPatch(patchAddr, patchBytes)) {
        InjectionRecord rec;
        rec.patchAddress = patchAddr;
        rec.patchSize = patchBytes.size();
        rec.originalBytes = m_mm.readBytes(patchAddr, patchBytes.size());
        rec.expectedBytes = patchBytes;
        m_injections[cheat.id] = rec;
        m_statusLabel->setText(QString("%1 enabled").arg(cheat.name));
    } else {
        m_statusLabel->setText(QString("Patch failed: %1").arg(cheat.name));
    }
}

// ── AOB + DB Action (one-shot) ──

void MainWindow::applyAobDbAction(const CheatDef& cheat) {
    QByteArray pattern, mask;
    pattern = parseAobPattern(cheat.aob, mask);

    quint64 foundAddr = m_mm.aobScanSingle(cheat.module, pattern, mask);
    if (foundAddr == 0) {
        m_statusLabel->setText(QString("AOB scan failed: %1").arg(cheat.name));
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    QByteArray patchBytes;
    QStringList bytes = QString(cheat.enableBytes).split(' ', Qt::SkipEmptyParts);
    for (const QString& b : bytes)
        patchBytes.append(static_cast<char>(b.toUInt(nullptr, 16)));

    quint64 patchAddr = foundAddr + cheat.patchOffset;

    if (m_mm.writeBytes(patchAddr, patchBytes)) {
        m_statusLabel->setText(QString("%1 applied").arg(cheat.name));
    } else {
        m_statusLabel->setText(QString("Patch failed: %1").arg(cheat.name));
    }
    m_statusLabel->setVisible(true);
    m_statusDot->setVisible(true);
}

// ── AOB + JMP Action (one-shot) ──

void MainWindow::applyAobJmpAction(const CheatDef& cheat) {
    QByteArray pattern, mask;
    pattern = parseAobPattern(cheat.aob, mask);

    quint64 injectAddr = m_mm.aobScanSingle(cheat.module, pattern, mask);
    if (injectAddr == 0) {
        m_statusLabel->setText(QString("AOB scan failed: %1").arg(cheat.name));
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    quint64 patchAddr = injectAddr + cheat.patchOffset;
    int nopCount = cheat.nopCount;
    int patchSize = 5 + nopCount;
    quint64 returnAddr = patchAddr + patchSize;

    quint64 globalAllocAddr = 0;
    QMap<QString, quint64> globalSymbols;
    if (cheat.globalAllocName && cheat.globalAllocSize > 0) {
        globalAllocAddr = m_mm.allocateRemote(cheat.globalAllocSize);
        if (globalAllocAddr == 0) { m_statusLabel->setText("Remote alloc failed"); return; }
        m_mm.writeBytes(globalAllocAddr, QByteArray(cheat.globalAllocSize, 0));
        globalSymbols[cheat.globalAllocName] = globalAllocAddr;
    }

    quint64 remoteAddr = m_mm.allocateRemote(0x1000);
    if (remoteAddr == 0) {
        m_statusLabel->setText("Could not allocate remote memory");
        if (globalAllocAddr) m_mm.freeRemote(globalAllocAddr);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    QByteArray shellcode = buildShellcode(cheat.newmemAsm, remoteAddr, returnAddr, globalAllocAddr, globalSymbols);
    if (shellcode.isEmpty()) {
        m_statusLabel->setText("Shellcode assembly failed");
        m_mm.freeRemote(remoteAddr);
        if (globalAllocAddr) m_mm.freeRemote(globalAllocAddr);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    m_mm.writeBytes(remoteAddr, shellcode);

    qint32 jmpOffset = static_cast<qint32>(remoteAddr) - static_cast<qint32>(patchAddr + 5);
    QByteArray jmpPatch;
    jmpPatch.append('\xE9');
    jmpPatch.append(reinterpret_cast<const char*>(&jmpOffset), 4);
    for (int i = 0; i < nopCount; i++) jmpPatch.append('\x90');

    if (m_mm.backupAndPatch(patchAddr, jmpPatch)) {
        m_statusLabel->setText(QString("%1 applied").arg(cheat.name));
    } else {
        m_statusLabel->setText("Patch injection failed");
    }
    m_statusLabel->setVisible(true);
    m_statusDot->setVisible(true);
}

// ── AOB + JMP Injection ──

void MainWindow::applyAobJmpInject(const CheatDef& cheat) {
    QByteArray pattern, mask;
    pattern = parseAobPattern(cheat.aob, mask);

    quint64 injectAddr = m_mm.aobScanSingle(cheat.module, pattern, mask);
    if (injectAddr == 0) {
        m_statusLabel->setText(QString("AOB scan failed: %1").arg(cheat.name));
        return;
    }

    quint64 patchAddr = injectAddr + cheat.patchOffset;
    int nopCount = cheat.nopCount;
    int patchSize = 5 + nopCount;
    quint64 returnAddr = patchAddr + patchSize;

    quint64 globalAllocAddr = 0;
    QMap<QString, quint64> globalSymbols;
    if (cheat.globalAllocName && cheat.globalAllocSize > 0) {
        globalAllocAddr = m_mm.allocateRemote(cheat.globalAllocSize);
        if (globalAllocAddr == 0) { m_statusLabel->setText("Remote alloc failed"); return; }
        m_mm.writeBytes(globalAllocAddr, QByteArray(cheat.globalAllocSize, 0));
        globalSymbols[cheat.globalAllocName] = globalAllocAddr;
    }

    quint64 remoteAddr = m_mm.allocateRemote(0x1000);
    if (remoteAddr == 0) {
        m_statusLabel->setText("Could not allocate remote memory");
        if (globalAllocAddr) m_mm.freeRemote(globalAllocAddr);
        return;
    }

    QByteArray shellcode = buildShellcode(cheat.newmemAsm, remoteAddr, returnAddr, globalAllocAddr, globalSymbols);
    if (shellcode.isEmpty()) {
        m_statusLabel->setText("Shellcode assembly failed");
        m_mm.freeRemote(remoteAddr);
        if (globalAllocAddr) m_mm.freeRemote(globalAllocAddr);
        return;
    }

    m_mm.writeBytes(remoteAddr, shellcode);

    qint32 jmpOffset = static_cast<qint32>(remoteAddr) - static_cast<qint32>(patchAddr + 5);
    QByteArray jmpPatch;
    jmpPatch.append('\xE9');
    jmpPatch.append(reinterpret_cast<const char*>(&jmpOffset), 4);
    for (int i = 0; i < nopCount; i++) jmpPatch.append('\x90');

    if (m_mm.backupAndPatch(patchAddr, jmpPatch)) {
        InjectionRecord rec;
        rec.patchAddress = patchAddr;
        rec.remoteCodeAddress = remoteAddr;
        rec.remoteCodeSize = 0x1000;
        rec.patchSize = patchSize;
        rec.originalBytes = m_mm.readBytes(patchAddr, patchSize);
        rec.expectedBytes = jmpPatch;
        m_injections[cheat.id] = rec;
        m_statusLabel->setText(QString("%1 enabled").arg(cheat.name));
    } else {
        m_statusLabel->setText("Patch injection failed");
        m_mm.freeRemote(remoteAddr);
        if (globalAllocAddr) m_mm.freeRemote(globalAllocAddr);
    }
}

// ── Terrain Editor (Thread Inject) ──

void MainWindow::applyTerrainEditor(const CheatDef& cheat) {
    // 1. Apply AOB patch if specified
    quint64 patchAddr = 0;
    QByteArray patchBytes;
    if (cheat.aob) {
        QByteArray aobPattern, aobMask;
        aobPattern = parseAobPattern(cheat.aob, aobMask);
        quint64 aobAddr = m_mm.aobScanSingle(cheat.module, aobPattern, aobMask);
        if (aobAddr == 0) {
            m_statusLabel->setText(QString("AOB scan failed: %1").arg(cheat.name));
            return;
        }

        patchAddr = aobAddr + cheat.patchOffset;
        patchBytes.append('\x89');
        patchBytes.append('\x81');
        patchBytes.append('\x88');
        patchBytes.append('\x00');
        patchBytes.append('\x00');
        patchBytes.append('\x00');

        if (!m_mm.backupAndPatch(patchAddr, patchBytes)) {
            m_statusLabel->setText("AOB patch failed");
            return;
        }
    }

    // 2. Read role_id from memory
    quint32 roleId = 0;
    if (cheat.roleIdAddress) {
        quint64 baseAddr = m_mm.parseAddress(cheat.roleIdAddress);
        if (baseAddr != 0) {
            if (cheat.roleIdOffset2 != 0) {
                quint32 ptr = m_mm.readUInt(baseAddr + cheat.roleIdOffset1);
                if (ptr != 0) {
                    roleId = m_mm.readUInt(ptr + cheat.roleIdOffset2);
                }
            } else {
                roleId = m_mm.readUInt(baseAddr + cheat.roleIdOffset1);
            }
        }
    }

    if (roleId == 0) {
        m_statusLabel->setText("Role ID not found");
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        if (patchAddr) m_mm.restorePatch(patchAddr);
        return;
    }

    // 3. Build JSON string (differs per cheat)
    int itemId = (cheat.id == 1807617779) ? 1105 : 10500;
    int itemNum = (cheat.id == 1807617779) ? 999 : 1;
    QString jsonStr = QString("{\"role_id\":%1,\"itemid\":%2,\"itemnum\":%3}")
        .arg(roleId).arg(itemId).arg(itemNum);
    QByteArray jsonBytes = jsonStr.toUtf8();
    jsonBytes.append('\0');
    while (jsonBytes.size() < 256) jsonBytes.append('\0');

    // 4. Command name string
    QByteArray cmdBytes = QByteArray("DEVELOPERSTORE_EXTRASTOREITEM_TOHOST");
    cmdBytes.append('\0');
    while (cmdBytes.size() < 256) cmdBytes.append('\0');

    // 5. Allocate remote buffers
    quint64 codeAddr = m_mm.allocateRemote(0x1000);
    if (codeAddr == 0) {
        m_statusLabel->setText("Remote alloc failed (code)");
        m_mm.restorePatch(patchAddr);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    quint64 jsonAddr = m_mm.allocateRemote(256);
    if (jsonAddr == 0) {
        m_statusLabel->setText("Remote alloc failed (json)");
        m_mm.freeRemote(codeAddr);
        m_mm.restorePatch(patchAddr);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    quint64 cmdAddr = m_mm.allocateRemote(256);
    if (cmdAddr == 0) {
        m_statusLabel->setText("Remote alloc failed (cmd)");
        m_mm.freeRemote(codeAddr);
        m_mm.freeRemote(jsonAddr);
        m_mm.restorePatch(patchAddr);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    // 6. Write strings to remote buffers
    m_mm.writeBytes(jsonAddr, jsonBytes);
    m_mm.writeBytes(cmdAddr, cmdBytes);

    // 7. Pre-resolve critical symbols
    quint64 sendToHostAddr = m_mm.resolvePdbSymbol("libSandboxEngine.dll", "SandBoxManager::sendToHost");
    if (sendToHostAddr == 0) sendToHostAddr = m_mm.resolveExport("libSandboxEngine.dll", "sendToHost");
    if (sendToHostAddr == 0) {
        m_statusLabel->setText("sendToHost not found");
        m_mm.freeRemote(codeAddr); m_mm.freeRemote(jsonAddr); m_mm.freeRemote(cmdAddr);
        m_mm.restorePatch(patchAddr);
        m_statusLabel->setVisible(true); m_statusDot->setVisible(true);
        return;
    }

    quint64 sleepAddr = m_mm.resolveExport("kernel32.dll", "Sleep");

    // 8. Assemble thread shellcode
    QMap<QString, quint64> globalSymbols;
    globalSymbols["XY"] = codeAddr;
    globalSymbols["string"] = jsonAddr;
    globalSymbols["string1"] = cmdAddr;
    globalSymbols["libSandboxEngine.SandBoxManager::sendToHost"] = sendToHostAddr;
    if (sleepAddr != 0) globalSymbols["kernel32.sleep"] = sleepAddr;

    QByteArray threadShellcode = buildShellcode(cheat.threadAsm, codeAddr, 0, 0, globalSymbols);
    if (threadShellcode.isEmpty()) {
        m_statusLabel->setText("Thread shellcode assembly failed");
        m_mm.freeRemote(codeAddr);
        m_mm.freeRemote(jsonAddr);
        m_mm.freeRemote(cmdAddr);
        m_mm.restorePatch(patchAddr);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    m_mm.writeBytes(codeAddr, threadShellcode);

    // 8. Create remote thread
    HANDLE hThread = CreateRemoteThread(m_mm.handle(), nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(codeAddr), nullptr, 0, nullptr);

    if (!hThread) {
        m_statusLabel->setText("CreateRemoteThread failed");
        m_mm.freeRemote(codeAddr);
        m_mm.freeRemote(jsonAddr);
        m_mm.freeRemote(cmdAddr);
        m_mm.restorePatch(patchAddr);
        m_statusLabel->setVisible(true);
        m_statusDot->setVisible(true);
        return;
    }

    // 9. Record everything
    InjectionRecord rec;
    rec.patchAddress = patchAddr;
    rec.patchSize = patchBytes.size();
    rec.originalBytes = m_mm.readBytes(patchAddr, patchBytes.size());
    rec.remoteCodeAddress = codeAddr;
    rec.remoteCodeSize = 0x1000;
    rec.remoteThread = hThread;
    rec.remoteDataAddress = jsonAddr;
    rec.remoteCmdAddress = cmdAddr;
    m_injections[cheat.id] = rec;

    m_statusLabel->setText(QString("%1 enabled").arg(cheat.name));
    m_statusLabel->setVisible(true);
    m_statusDot->setVisible(true);
}

// ── Always On Top ──

void MainWindow::applyReadme() {
    QFile file(":/README.md");
    QString content;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(file.readAll());
    } else {
        content = "Zelvex v1.0\nDeveloped by nyxdev_\n\nAll cheat functions were tested on MiniGameApp.exe\nUse at your own risk.";
    }
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Zelvex README");
    msgBox.setTextFormat(Qt::MarkdownText);
    msgBox.setText(content);
    msgBox.exec();
}

// ── Teleport Manager ──

bool MainWindow::isValidTeleportValue(float val) const {
    if (std::isnan(val) || std::isinf(val)) return false;
    if (val < -100000.0f || val > 100000.0f) return false;
    return true;
}

void MainWindow::loadSavedTeleports() {
    QSettings settings("Zelvex", "Zelvex");
    int count = settings.value("teleport/count", 0).toInt();
    m_savedTeleports.clear();
    for (int i = 0; i < count; i++) {
        QString name = settings.value(QString("teleport/%1/name").arg(i)).toString();
        float x = settings.value(QString("teleport/%1/x").arg(i)).toFloat();
        float y = settings.value(QString("teleport/%1/y").arg(i)).toFloat();
        float z = settings.value(QString("teleport/%1/z").arg(i)).toFloat();
        if (!isValidTeleportValue(x) || !isValidTeleportValue(y) || !isValidTeleportValue(z))
            continue;
        m_savedTeleports.append({name, {x, y, z}});
    }
    refreshTeleportList();
}

void MainWindow::saveTeleport(const QString& name) {
    if (!m_mm.isAttached() || name.isEmpty()) return;

    quint64 base = m_mm.parseAddress("libSandboxEngine.dll+27772F8");
    if (base == 0) return;

    quint64 xAddr = m_mm.resolvePointer(base, {0xe8, 0x270});
    quint64 yAddr = m_mm.resolvePointer(base, {0xe0, 0x270});
    quint64 zAddr = m_mm.resolvePointer(base, {0xe4, 0x270});
    if (!xAddr || !yAddr || !zAddr) return;

    float x = m_mm.readFloat(xAddr);
    float y = m_mm.readFloat(yAddr);
    float z = m_mm.readFloat(zAddr);

    if (!isValidTeleportValue(x) || !isValidTeleportValue(y) || !isValidTeleportValue(z)) {
        m_statusLabel->setText("Invalid position values");
        return;
    }

    m_savedTeleports.append({name, {x, y, z}});

    QSettings settings("Zelvex", "Zelvex");
    settings.setValue("teleport/count", m_savedTeleports.size());
    int idx = m_savedTeleports.size() - 1;
    settings.setValue(QString("teleport/%1/name").arg(idx), name);
    settings.setValue(QString("teleport/%1/x").arg(idx), x);
    settings.setValue(QString("teleport/%1/y").arg(idx), y);
    settings.setValue(QString("teleport/%1/z").arg(idx), z);

    refreshTeleportList();
    m_statusLabel->setText(QString("%1: %2").arg(QString::fromUtf8(lang(m_currentLanguage).save)).arg(name));
}

void MainWindow::deleteTeleport(int index) {
    if (index < 0 || index >= m_savedTeleports.size()) return;
    m_savedTeleports.removeAt(index);

    QSettings settings("Zelvex", "Zelvex");
    settings.setValue("teleport/count", m_savedTeleports.size());
    for (int i = 0; i < m_savedTeleports.size(); i++) {
        settings.setValue(QString("teleport/%1/name").arg(i), m_savedTeleports[i].first);
        settings.setValue(QString("teleport/%1/x").arg(i), m_savedTeleports[i].second[0]);
        settings.setValue(QString("teleport/%1/y").arg(i), m_savedTeleports[i].second[1]);
        settings.setValue(QString("teleport/%1/z").arg(i), m_savedTeleports[i].second[2]);
    }
    refreshTeleportList();
}

void MainWindow::teleportToSaved(int index) {
    if (!m_mm.isAttached() || index < 0 || index >= m_savedTeleports.size()) return;
    const auto& tp = m_savedTeleports[index];

    if (!isValidTeleportValue(tp.second[0]) || !isValidTeleportValue(tp.second[1]) || !isValidTeleportValue(tp.second[2]))
        return;

    quint64 base = m_mm.parseAddress("libSandboxEngine.dll+27772F8");
    if (base == 0) return;

    quint64 xAddr = m_mm.resolvePointer(base, {0xe8, 0x270});
    quint64 yAddr = m_mm.resolvePointer(base, {0xe0, 0x270});
    quint64 zAddr = m_mm.resolvePointer(base, {0xe4, 0x270});

    if (xAddr) m_mm.writeFloat(xAddr, tp.second[0]);
    if (yAddr) m_mm.writeFloat(yAddr, tp.second[1]);
    if (zAddr) m_mm.writeFloat(zAddr, tp.second[2]);

    m_statusLabel->setText(QString("Teleported to: %1").arg(tp.first));
}

void MainWindow::startRandomTeleport() {
    if (!m_mm.isAttached()) return;
    if (m_savedTeleports.size() < 2) {
        m_statusLabel->setText("Need 2+ saved positions for random TP");
        return;
    }

    quint64 base = m_mm.parseAddress("libSandboxEngine.dll+27772F8");
    if (base == 0) return;

    quint64 xAddr = m_mm.resolvePointer(base, {0xe8, 0x270});
    quint64 yAddr = m_mm.resolvePointer(base, {0xe0, 0x270});
    quint64 zAddr = m_mm.resolvePointer(base, {0xe4, 0x270});
    if (!xAddr || !yAddr || !zAddr) return;

    m_randomTpSavedPos[0] = m_mm.readFloat(xAddr);
    m_randomTpSavedPos[1] = m_mm.readFloat(yAddr);
    m_randomTpSavedPos[2] = m_mm.readFloat(zAddr);

    if (!isValidTeleportValue(m_randomTpSavedPos[0]) || !isValidTeleportValue(m_randomTpSavedPos[1]) || !isValidTeleportValue(m_randomTpSavedPos[2])) {
        m_statusLabel->setText("Cannot start: current position is invalid");
        return;
    }

    if (!m_randomTpTimer) {
        m_randomTpTimer = new QTimer(this);
        connect(m_randomTpTimer, &QTimer::timeout, this, &MainWindow::onRandomTpTick);
    }
    m_randomTpTimer->setInterval(1000);
    m_randomTpTimer->start();
    m_randomTpActive = true;
    m_statusLabel->setText("Random Teleport enabled");
}

void MainWindow::stopRandomTeleport() {
    if (m_randomTpTimer) m_randomTpTimer->stop();
    m_randomTpActive = false;

    if (m_mm.isAttached()) {
        quint64 base = m_mm.parseAddress("libSandboxEngine.dll+27772F8");
        if (base != 0) {
            quint64 xAddr = m_mm.resolvePointer(base, {0xe8, 0x270});
            quint64 yAddr = m_mm.resolvePointer(base, {0xe0, 0x270});
            quint64 zAddr = m_mm.resolvePointer(base, {0xe4, 0x270});
            if (xAddr) m_mm.writeFloat(xAddr, m_randomTpSavedPos[0]);
            if (yAddr) m_mm.writeFloat(yAddr, m_randomTpSavedPos[1]);
            if (zAddr) m_mm.writeFloat(zAddr, m_randomTpSavedPos[2]);
        }
    }
    m_statusLabel->setText("Random Teleport disabled  - position restored");
}

void MainWindow::onRandomTpTick() {
    if (!m_mm.isAttached() || !m_randomTpActive) {
        stopRandomTeleport();
        return;
    }
    if (m_savedTeleports.size() < 2) {
        stopRandomTeleport();
        return;
    }

    QList<int> validIndices;
    for (int i = 0; i < m_savedTeleports.size(); i++) {
        const auto& tp = m_savedTeleports[i];
        if (isValidTeleportValue(tp.second[0]) && isValidTeleportValue(tp.second[1]) && isValidTeleportValue(tp.second[2]))
            validIndices.append(i);
    }
    if (validIndices.size() < 2) {
        stopRandomTeleport();
        m_statusLabel->setText("Random TP stopped: not enough valid positions");
        return;
    }

    int idx = validIndices[rand() % validIndices.size()];
    const auto& tp = m_savedTeleports[idx];

    quint64 base = m_mm.parseAddress("libSandboxEngine.dll+27772F8");
    if (base == 0) return;

    quint64 xAddr = m_mm.resolvePointer(base, {0xe8, 0x270});
    quint64 yAddr = m_mm.resolvePointer(base, {0xe0, 0x270});
    quint64 zAddr = m_mm.resolvePointer(base, {0xe4, 0x270});

    if (xAddr) m_mm.writeFloat(xAddr, tp.second[0]);
    if (yAddr) m_mm.writeFloat(yAddr, tp.second[1]);
    if (zAddr) m_mm.writeFloat(zAddr, tp.second[2]);
}

void MainWindow::onSaveTeleportClicked() {
    if (m_tpNameInput && !m_tpNameInput->text().isEmpty()) {
        saveTeleport(m_tpNameInput->text());
        m_tpNameInput->clear();
    }
}

void MainWindow::refreshTeleportList() {
    if (!m_tpListLayout) return;
    while (m_tpListLayout->count() > 0) {
        QLayoutItem* item = m_tpListLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    for (int i = 0; i < m_savedTeleports.size(); i++) {
        auto* card = new QWidget();
        card->setObjectName("cheatCard");
        card->setMinimumHeight(48);
        auto* h = new QHBoxLayout(card);
        h->setContentsMargins(16, 8, 16, 8);
        h->setSpacing(8);

        auto* nameLabel = new QLabel(m_savedTeleports[i].first);
        nameLabel->setStyleSheet("color: #e6e9ee; font-size: 12px; font-weight: 600;");
        h->addWidget(nameLabel, 1);

        auto* coordLabel = new QLabel(QString("(%1, %2, %3)")
            .arg(m_savedTeleports[i].second[0], 0, 'f', 1)
            .arg(m_savedTeleports[i].second[1], 0, 'f', 1)
            .arg(m_savedTeleports[i].second[2], 0, 'f', 1));
        coordLabel->setStyleSheet("color: #677080; font-size: 10px;");
        h->addWidget(coordLabel);

        auto* tpBtn = new QPushButton("TP");
        tpBtn->setObjectName("actionButton");
        tpBtn->setFixedSize(40, 26);
        connect(tpBtn, &QPushButton::clicked, this, [this, i]() { teleportToSaved(i); });
        h->addWidget(tpBtn);

        auto* delBtn = new QPushButton("X");
        delBtn->setObjectName("windowBtnClose");
        delBtn->setFixedSize(26, 26);
        connect(delBtn, &QPushButton::clicked, this, [this, i]() { deleteTeleport(i); });
        h->addWidget(delBtn);

        m_tpListLayout->addWidget(card);
    }

    if (m_savedTeleports.isEmpty()) {
        auto* emptyLabel = new QLabel("No saved positions yet");
        emptyLabel->setStyleSheet("color: #5f6875; font-size: 11px; padding: 12px;");
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_tpListLayout->addWidget(emptyLabel);
    }
}

// ── Give Item ──

void MainWindow::giveItem(int itemId, int quantity) {
    if (m_giveItemBusy) return;
    if (itemId <= 0 || quantity <= 0) {
        appendLuaOutput("Invalid item id / quantity");
        return;
    }
    if (!m_mm.isAttached()) {
        appendLuaOutput("Attach to the game first");
        return;
    }

    // Route through the Lua engine's native.giveItem (runs inside the game via
    // the injected DLL)  - the old remote-thread shellcode path is unreliable.
    m_giveItemBusy = true;
    if (!m_mm.getSharedMemory()) {
        appendLuaOutput("DLL not injected - injecting now...");
        if (!injectLuaDll()) {
            appendLuaOutput("DLL injection failed - attach to the game and try again");
            m_giveItemBusy = false;
            return;
        }
    }
    QString code = QString("zout('[give] ' .. tostring(native.giveItem(%1,%2)))")
                       .arg(itemId).arg(quantity);
    executeLuaViaDll(code);
    m_giveItemBusy = false;
}

void MainWindow::applyGiveItem() {
    if (!m_mm.isAttached()) {
        appendLuaOutput("Attach to the game first");
        return;
    }

    bool ok = false;
    int itemId = m_itemIdInput ? m_itemIdInput->text().toInt(&ok) : 0;
    if (!ok || itemId <= 0) {
        appendLuaOutput("Enter a valid item ID");
        return;
    }

    int quantity = 1;
    if (m_itemQtyInput) {
        quantity = m_itemQtyInput->text().toInt(&ok);
        if (!ok || quantity <= 0) quantity = 1;
        if (quantity > 999) quantity = 999;
    }

    giveItem(itemId, quantity);
}

// ── Items Tab (lazy-loaded) ──

void MainWindow::loadItemsTab() {
    if (m_itemsTabLoaded) return;

    auto* scroll = qobject_cast<QScrollArea*>(m_stack->widget(7));
    if (!scroll) return;
    auto* page = scroll->widget();
    if (!page) return;
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    if (!layout) return;

    while (layout->count() > 1) {
        QLayoutItem* item = layout->takeAt(0);
        if (item) { if (item->widget()) delete item->widget(); delete item; }
    }

    auto* header = createPageHeader(trSection("Item Giver"));
    layout->insertWidget(layout->count() - 1, header);

    // ── Toolbar: category / search / id / qty / give ──
    auto* toolbarCard = new QWidget();
    toolbarCard->setObjectName("cheatCard");
    toolbarCard->setMinimumHeight(46);
    auto* toolbarLayout = new QHBoxLayout(toolbarCard);
    toolbarLayout->setContentsMargins(14, 8, 14, 8);
    toolbarLayout->setSpacing(8);

    m_itemCategoryCombo = new QComboBox();
    m_itemCategoryCombo->setObjectName("toolCombo");
    m_itemCategoryCombo->setMinimumWidth(150);
    m_itemCategoryCombo->addItem("All Categories");
    for (const auto& cat : m_itemDatabase)
        m_itemCategoryCombo->addItem(cat.name);
    m_itemCategoryCombo->setMaxVisibleItems(12);
    connect(m_itemCategoryCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        m_currentItemCategory = (text == "All Categories") ? "" : text;
        m_currentItemPage = 1;
        updateItemGrid();
    });
    toolbarLayout->addWidget(m_itemCategoryCombo);

    m_itemSearchInput = new QLineEdit();
    m_itemSearchInput->setObjectName("valueInput");
    m_itemSearchInput->setPlaceholderText("Search item...");
    m_itemSearchInput->setClearButtonEnabled(true);
    m_itemSearchInput->setMinimumWidth(170);
    connect(m_itemSearchInput, &QLineEdit::textChanged, this, [this]() {
        m_currentItemPage = 1;
        updateItemGrid();
    });
    toolbarLayout->addWidget(m_itemSearchInput, 1);

    m_itemIdInput = new QLineEdit();
    m_itemIdInput->setObjectName("valueInput");
    m_itemIdInput->setPlaceholderText("Item ID");
    m_itemIdInput->setFixedWidth(100);
    m_itemIdInput->setValidator(new QIntValidator(1, 999999999, m_itemIdInput));
    toolbarLayout->addWidget(m_itemIdInput);

    m_itemQtyInput = new QLineEdit("1");
    m_itemQtyInput->setObjectName("valueInput");
    m_itemQtyInput->setPlaceholderText("Qty");
    m_itemQtyInput->setFixedWidth(56);
    m_itemQtyInput->setValidator(new QIntValidator(1, 999, m_itemQtyInput));
    toolbarLayout->addWidget(m_itemQtyInput);

    auto* giveBtn = new QPushButton("GIVE");
    giveBtn->setObjectName("primaryButton");
    giveBtn->setFixedHeight(30);
    giveBtn->setCursor(Qt::PointingHandCursor);
    connect(giveBtn, &QPushButton::clicked, this, &MainWindow::applyGiveItem);
    toolbarLayout->addWidget(giveBtn);

    layout->insertWidget(layout->count() - 1, toolbarCard);

    // ── Item grid ──
    m_itemGridWidget = new QWidget();
    m_itemGridWidget->setObjectName("cheatCard");
    m_itemGridLayout = new QGridLayout(m_itemGridWidget);
    m_itemGridLayout->setContentsMargins(10, 10, 10, 10);
    m_itemGridLayout->setSpacing(8);
    layout->insertWidget(layout->count() - 1, m_itemGridWidget);

    // ── Pagination ──
    auto* pagerCard = new QWidget();
    pagerCard->setObjectName("cheatCard");
    pagerCard->setMinimumHeight(40);
    auto* pagerLayout = new QHBoxLayout(pagerCard);
    pagerLayout->setContentsMargins(14, 6, 14, 6);
    pagerLayout->setSpacing(8);

    auto* prevBtn = new QPushButton("PREV");
    prevBtn->setObjectName("ghostButton");
    prevBtn->setFixedHeight(26);
    connect(prevBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentItemPage > 1) { m_currentItemPage--; updateItemGrid(); }
    });
    pagerLayout->addWidget(prevBtn);

    m_itemsPageLabel = new QLabel("Page 1 / 1");
    m_itemsPageLabel->setStyleSheet("color: #5f6875; font-size: 10px; font-weight: 600; background: transparent; border: none;");
    m_itemsPageLabel->setAlignment(Qt::AlignCenter);
    pagerLayout->addWidget(m_itemsPageLabel, 1);

    auto* nextBtn = new QPushButton("NEXT");
    nextBtn->setObjectName("ghostButton");
    nextBtn->setFixedHeight(26);
    connect(nextBtn, &QPushButton::clicked, this, [this]() {
        m_currentItemPage++;
        updateItemGrid();
    });
    pagerLayout->addWidget(nextBtn);
    layout->insertWidget(layout->count() - 1, pagerCard);

    auto* hintLabel = new QLabel("TIP:  gifts are delivered through the Lua engine (native.giveItem)  - inject the DLL on the Lua tab first if it is not already loaded");
    hintLabel->setStyleSheet("font-size: 10px; color: #5f6875; background: transparent; border: none;");
    layout->insertWidget(layout->count() - 1, hintLabel);

    // ── Zelvex Native Items (DLL) ──
    {
        auto nativeCmdRow = [this](const QString& title, const QString& sub,
                                  const QList<QPair<QString, QString>>& actions) -> QWidget* {
            auto* widget = new QWidget();
            widget->setObjectName("cheatCard");
            widget->setMinimumHeight(52);
            auto* row = new QHBoxLayout(widget);
            row->setContentsMargins(16, 10, 16, 10);
            row->setSpacing(8);
            auto* textW = new QWidget();
            auto* textL = new QVBoxLayout(textW);
            textL->setContentsMargins(0, 0, 0, 0);
            textL->setSpacing(2);
            auto* nameL = new QLabel(title);
            nameL->setObjectName("toggleName");
            textL->addWidget(nameL);
            if (!sub.isEmpty()) {
                auto* subL = new QLabel(sub);
                subL->setObjectName("toggleSubtitle");
                textL->addWidget(subL);
            }
            row->addWidget(textW, 1);
            for (const auto& a : actions) {
                auto* b = new QPushButton(a.first);
                b->setObjectName("ghostButton");
                b->setFixedHeight(28);
                QString cmd = a.second;
                connect(b, &QPushButton::clicked, this, [this, cmd]() {
                    executeLuaViaDll(cmd);
                });
                row->addWidget(b);
            }
            return widget;
        };
        auto* nativeHeader = createPageHeader(trSection("Zelvex Native Items"));
        layout->insertWidget(layout->count() - 1, nativeHeader);
        layout->insertWidget(layout->count() - 1,
            nativeCmdRow("Sort Pack", "Automatically sort your inventory",
                {{"SORT", "native.sortPack()"}}));
        layout->insertWidget(layout->count() - 1,
            nativeCmdRow("Repair All", "Repair every item in your inventory",
                {{"REPAIR", "native.repairAll()"}}));
        layout->insertWidget(layout->count() - 1,
            nativeCmdRow("Discard All", "Throw away all items in your inventory",
                {{"DISCARD", "native.discardAll()"}}));
        layout->insertWidget(layout->count() - 1,
            nativeCmdRow("Unlock Locked Items", "Zero the lock flag on item unlocks (CT recipe)",
                {{"ON", "native.unlockItems(1)"}, {"OFF", "native.unlockItems(0)"}}));
        layout->insertWidget(layout->count() - 1,
            nativeCmdRow("Infinite Durability", "Re-repairs every slot every second (STOP to end)",
                {{"START", "native.echo(0) while true do Item:repairAll() wait(1) end"}}));
    }

    m_currentItemCategory = "";
    m_currentItemPage = 1;
    m_itemsTabLoaded = true;
    updateItemGrid();
}

// ── Lua Tab (lazy-loaded) ──

void MainWindow::loadLuaTab() {
    if (m_luaTabLoaded) return;

    auto* scroll = qobject_cast<QScrollArea*>(m_stack->widget(8));
    if (!scroll) return;
    auto* page = scroll->widget();
    if (!page) return;
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    if (!layout) return;

    while (layout->count() > 1) {
        QLayoutItem* item = layout->takeAt(0);
        if (item) { if (item->widget()) delete item->widget(); delete item; }
    }

    auto* header = createPageHeader(trSection("Lua Code Injector"));
    layout->insertWidget(layout->count() - 1, header);
    header->setVisible(false);

    // ── Toolbar: dominant ATTACH, labeled Game/Runtime statuses, INJECT ──
    auto* toolbarCard = new QWidget();
    toolbarCard->setObjectName("luaToolbar");
    toolbarCard->setMinimumHeight(38);
    auto* toolbarLayout = new QHBoxLayout(toolbarCard);
    toolbarLayout->setContentsMargins(12, 4, 12, 4);
    toolbarLayout->setSpacing(6);

    m_luaAttachBtn = new QPushButton("ATTACH GAME");
    m_luaAttachBtn->setObjectName("primaryButton");
    m_luaAttachBtn->setMinimumWidth(110);
    m_luaAttachBtn->setFixedHeight(32);
    m_luaAttachBtn->setToolTip("Attach to the game process");
    connect(m_luaAttachBtn, &QPushButton::clicked, this, &MainWindow::onAttachClicked);
    toolbarLayout->addWidget(m_luaAttachBtn);

    m_luaDetachBtn = new QPushButton("DETACH");
    m_luaDetachBtn->setObjectName("dangerGhostButton");
    m_luaDetachBtn->setMinimumWidth(66);
    m_luaDetachBtn->setFixedHeight(32);
    m_luaDetachBtn->setToolTip("Release the game process");
    connect(m_luaDetachBtn, &QPushButton::clicked, this, &MainWindow::onAttachClicked);
    toolbarLayout->addWidget(m_luaDetachBtn);

    toolbarLayout->addStretch();

    auto* injectBtn = new QPushButton("INJECT DLL");
    injectBtn->setObjectName("amberButton");
    injectBtn->setFixedHeight(32);
    injectBtn->setToolTip("Inject the Lua VM DLL into the attached process");
    connect(injectBtn, &QPushButton::clicked, this, &MainWindow::injectLuaDll);
    toolbarLayout->addWidget(injectBtn);

    layout->insertWidget(layout->count() - 1, toolbarCard);

    refreshLuaAttachState();

    // ── Editor (left) | Console (right) ──
    m_luaSplitter = new QSplitter(Qt::Horizontal);
    m_luaSplitter->setChildrenCollapsible(false);
    m_luaSplitter->setHandleWidth(3);
    m_luaSplitter->setStyleSheet(
        "QSplitter::handle { background-color: #1b212b; }"
        "QSplitter::handle:hover { background-color: #3d8bff; }");

    auto* editorCard = new QWidget();
    editorCard->setObjectName("cheatCard");
    auto* editorCardLayout = new QVBoxLayout(editorCard);
    editorCardLayout->setContentsMargins(10, 10, 10, 10);
    editorCardLayout->setSpacing(8);

    auto* editorToolbar = new QWidget();
    auto* editorToolbarLayout = new QVBoxLayout(editorToolbar);
    editorToolbarLayout->setContentsMargins(0, 0, 0, 0);
    editorToolbarLayout->setSpacing(4);

    // ── Row 1: EXECUTE + STOP + stretch + Examples ──
    auto* primaryRow = new QWidget();
    auto* primaryLayout = new QHBoxLayout(primaryRow);
    primaryLayout->setContentsMargins(0, 0, 0, 0);
    primaryLayout->setSpacing(8);

    auto* execBtn = new QPushButton(QString::fromUtf8(lang(m_currentLanguage).luaExecute));
    execBtn->setObjectName("primaryButton");
    execBtn->setFixedWidth(96);
    execBtn->setFixedHeight(30);
    execBtn->setToolTip("Run the current script (Ctrl+Enter)");
    connect(execBtn, &QPushButton::clicked, this, &MainWindow::executeLuaCode);
    primaryLayout->addWidget(execBtn);

    m_luaStopBtn = new QPushButton("STOP");
    m_luaStopBtn->setObjectName("stopButton");
    m_luaStopBtn->setFixedWidth(48);
    m_luaStopBtn->setFixedHeight(30);
    m_luaStopBtn->setEnabled(false);
    m_luaStopBtn->setVisible(false);
    m_luaStopBtn->setToolTip("Abort the running script (loops die gracefully at their next check)");
    connect(m_luaStopBtn, &QPushButton::clicked, this, &MainWindow::stopLuaScript);
    primaryLayout->addWidget(m_luaStopBtn);

    primaryLayout->addStretch();

    m_luaExampleCombo = new QComboBox();
    m_luaExampleCombo->setObjectName("toolCombo");
    m_luaExampleCombo->setMinimumWidth(120);
    m_luaExampleCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_luaExampleCombo->setMaxVisibleItems(12);
    m_luaExampleCombo->setToolTip("Preloaded example scripts (select to load into this tab)");
    m_luaExampleCombo->addItem("Examples...");
    const QStringList examples = {
        "Godmode + auto-heal",
        "Star farmer",
        "Player radar (ESP scan)",
        "Teleport to nearest player",
        "Bring player to you",
        "Kill aura 15s demo",
        "Aimbot 10s demo",
        "Auto-mine farm (60s)",
        "Packet crafter (give item)",
        "Lua showcase (_VERSION)",
        "Doctor - what works on your build"
    };
    for (const QString& e : examples) m_luaExampleCombo->addItem(e);
    connect(m_luaExampleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int i) {
        if (i <= 0 || !m_luaCodeEditor) return;
        QString code;
        switch (i) {
        case 1: // Godmode
            code =
                "-- Godmode: full HP forever + auto-revive + nothing drops.\n"
                "Player:noDrop(true)\n"
                "while true do\n"
                "    Player:setHealth(9999)\n"
                "    if Player:getHp() < 10 then Player:revive(2) end\n"
                "    wait(0.2)\n"
                "end\n";
            break;
        case 2: // Star farmer
            code =
                "-- Collects a star every second, forever.\n"
                "while true do\n"
                "    Player:addStar(1)\n"
                "    print(\"star collected\")\n"
                "    wait(1)\n"
                "end\n";
            break;
        case 3: // Player radar
            code =
                "-- Live player radar: UID, position and team of everyone.\n"
                "while true do\n"
                "    print((\"--- scan @ %s ---\"):format(os.date(\"%H:%M:%S\")))\n"
                "    for _, p in ipairs(Players.list()) do\n"
                "        print((\"uid=%d pos=(%d,%d,%d) team=%d\")\n"
                "            :format(p.uid, p.x, p.y, p.z, p.team))\n"
                "    end\n"
                "    wait(2)\n"
                "end\n";
            break;
        case 4: // Teleport to nearest player
            code =
                "-- Teleport onto the nearest player every 3 seconds.\n"
                "-- Reports the REAL result - failures are not hidden.\n"
                "while true do\n"
                "    local p = Players.nearest(5000)\n"
                "    if p then\n"
                "        local ok, err = Player:teleportTo(p.uid, 0, 3, 0)\n"
                "        if ok then\n"
                "            print((\"on top of %d\"):format(p.uid))\n"
                "        else\n"
                "            print((\"tp FAILED for %d: %s\")\n"
                "                :format(p.uid, tostring(err)))\n"
                "        end\n"
                "    else\n"
                "        print(\"no player within 5000 blocks\")\n"
                "    end\n"
                "    wait(3)\n"
                "end\n";
            break;
        case 5: // Bring player to you
            code =
                "-- Drag the nearest player to you every tick (host/LAN best).\n"
                "-- Run Player:bringPlayer(0) to stop dragging.\n"
                "local p = Players.nearest(3000)\n"
                "if p then\n"
                "    local ok, err = Player:bringPlayer(p.uid)\n"
                "    print(ok and (\"dragging \" .. p.uid)\n"
                "              or (\"bring FAILED: \" .. tostring(err)))\n"
                "else\n"
                "    print(\"no player within 3000 blocks\")\n"
                "end\n";
            break;
        case 6: // Kill aura demo
            code =
                "-- Kill aura mode 2 for 15 seconds, then off.\n"
                "Actor:killAura(2)\n"
                "print(\"kill aura ON\")\n"
                "wait(15)\n"
                "Actor:killAura(0)\n"
                "print(\"kill aura OFF\")\n";
            break;
        case 7: // Aimbot demo
            code =
                "-- Aimbot for 10 seconds, then off.\n"
                "Actor:aimbot(true, 3000)\n"
                "print(\"aimbot locked (range 3000)\")\n"
                "wait(10)\n"
                "Actor:aimbot(false)\n"
                "print(\"aimbot released\")\n";
            break;
        case 8: // Auto-mine farm
            code =
                "-- Auto-mine nearby ores; toggles off after 60 seconds.\n"
                "Actor:mineAll(true)\n"
                "print(\"mining...\")\n"
                "wait(60)\n"
                "Actor:mineAll(false)\n"
                "print(\"done\")\n";
            break;
        case 9: // Packet crafter
            code =
                "-- Raw packet crafter: give 1x item 1105 to YOURSELF via host.\n"
                "-- Change itemid / itemnum as you like. Host sees the gift.\n"
                "local uid = Player:getUid()\n"
                "Net.send(\"DEVELOPERSTORE_EXTRASTOREITEM_TOHOST\",\n"
                "    string.format('{\"role_id\":%d,\"itemid\":1105,\"itemnum\":1}', uid))\n"
                "print(\"packet sent to \" .. uid)\n";
            break;
        case 10: // Lua showcase
            code =
                "-- Full real Lua: functions, tables, math, string lib.\n"
                "local function dist(x1,y1,z1,x2,y2,z2)\n"
                "    return math.sqrt((x1-x2)^2+(y1-y2)^2+(z1-z2)^2)\n"
                "end\n"
                "local me = {Player:getPos()}\n"
                "for _, p in ipairs(Players.list()) do\n"
                "    print((\"player %d is %.0f blocks away\")\n"
                "        :format(p.uid, dist(me[1],me[2],me[3],p.x,p.y,p.z)))\n"
                "end\n"
                "print((\"running %s - REAL Lua inside Zelvex\"):format(_VERSION))\n";
            break;
        case 11: // Doctor
            code =
                "-- Doctor: checks every subsystem on YOUR game build.\n"
                "-- Run this FIRST whenever something misbehaves, and\n"
                "-- paste the output when reporting a problem.\n"
                "local s = native.state() or \"\"\n"
                "if s == \"\" then print(\"state() failed - attached?\") return end\n"
                "for tok in s:gmatch(\"%S+\") do\n"
                "    local k, v = tok:match(\"^(%w+)=(.+)$\")\n"
                "    if k then\n"
                "        local good = (v == \"ok\") or (v == \"1\")\n"
                "        local bad  = (v == \"missing\") or (v == \"no\") or (v == \"0\")\n"
                "        local mark = good and \"[OK]  \" or (bad and \"[BAD] \" or \"[??]  \")\n"
                "        print(mark .. k .. \" = \" .. v)\n"
                "    end\n"
                "end\n"
                "print(\"---\")\n"
                "print(\"players visible: \" .. tostring(Players.count()))\n"
                "print(\"uid: \" .. tostring(Player:getUid()))\n"
                "print(\"pos: \" .. tostring(Player:getPos()))\n"
                "print(\"Interpretation: BAD interact/teleHook => kill+tp-to-player\"\n"
                "      .. \" will fail; BAD roomKick => kick unavailable this build.\")\n";
            break;
        }
        if (!code.isEmpty()) {
            m_luaCodeEditor->setPlainText(code);
            m_luaExampleCombo->setCurrentIndex(0);
            appendLuaOutput("Loaded example: " + m_luaExampleCombo->itemText(i));
        }
    });
    primaryLayout->addWidget(m_luaExampleCombo);

    editorToolbarLayout->addWidget(primaryRow);

    // ── Row 2: CLEAR + COPY + SAVE + LOAD ──
    auto* secondaryRow = new QWidget();
    auto* secondaryLayout = new QHBoxLayout(secondaryRow);
    secondaryLayout->setContentsMargins(0, 0, 0, 0);
    secondaryLayout->setSpacing(8);

    auto* clearBtn = new QPushButton(QString::fromUtf8(lang(m_currentLanguage).luaClear));
    clearBtn->setObjectName("ghostButton");
    clearBtn->setFixedWidth(48);
    clearBtn->setFixedHeight(28);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_luaCodeEditor) m_luaCodeEditor->clear();
    });
    secondaryLayout->addWidget(clearBtn);

    auto* copyBtn = new QPushButton("COPY");
    copyBtn->setObjectName("ghostButton");
    copyBtn->setFixedWidth(46);
    copyBtn->setFixedHeight(28);
    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        if (m_luaCodeEditor && m_luaCodeEditor->textCursor().hasSelection())
            QApplication::clipboard()->setText(
                m_luaCodeEditor->textCursor().selectedText().replace(QChar(0x2029), "\n"));
    });
    secondaryLayout->addWidget(copyBtn);

    auto* saveBtn = new QPushButton("SAVE");
    saveBtn->setObjectName("ghostButton");
    saveBtn->setFixedWidth(46);
    saveBtn->setFixedHeight(28);
    saveBtn->setToolTip("Save this script to a .lua file");
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        if (!m_luaCodeEditor) return;
        QString path = m_scriptFiles.isEmpty() ? QString() : m_scriptFiles[0];
        if (path.isEmpty()) {
            path = QFileDialog::getSaveFileName(this, "Save script",
                QDir::homePath() + "/scripts", "Lua scripts (*.lua);;All files (*)");
            if (path.isEmpty()) return;
            if (!path.endsWith(".lua", Qt::CaseInsensitive)) path += ".lua";
            if (!m_scriptFiles.isEmpty()) m_scriptFiles[0] = path;
        }
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(m_luaCodeEditor->toPlainText().toUtf8());
            appendLuaOutput("Saved: " + path);
        } else {
            appendLuaOutput("Save failed: " + path);
        }
    });
    secondaryLayout->addWidget(saveBtn);

    auto* loadBtn = new QPushButton("LOAD");
    loadBtn->setObjectName("ghostButton");
    loadBtn->setFixedWidth(46);
    loadBtn->setFixedHeight(28);
    loadBtn->setToolTip("Load a .lua file into the editor");
    connect(loadBtn, &QPushButton::clicked, this, [this]() {
        if (!m_luaCodeEditor) return;
        QString path = QFileDialog::getOpenFileName(this, "Load script",
            QDir::homePath() + "/scripts", "Lua scripts (*.lua);;All files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_luaCodeEditor->setPlainText(QString::fromUtf8(f.readAll()));
            if (!m_scriptFiles.isEmpty()) m_scriptFiles[0] = path;
            appendLuaOutput("Loaded: " + path);
        } else {
            appendLuaOutput("Load failed: " + path);
        }
    });
    secondaryLayout->addWidget(loadBtn);

    secondaryLayout->addStretch();

    editorToolbarLayout->addWidget(secondaryRow);

    editorCardLayout->addWidget(editorToolbar);

    // Single editor (no script tabs — SAVE/LOAD handles files)
    auto* ed = new LuaEditor();
    ed->setObjectName("luaEditor");
    ed->setMinimumHeight(100);
    ed->setPlaceholderText(
        "-- REAL LUA + Mini World API:\n"
        "--   Player:setHealth(9999)      Player:noclip(true)\n"
        "--   local x,y,z = Player:getPos()\n"
        "--   for _,p in ipairs(Players.list()) do print(p.uid,p.x,p.y,p.z) end\n"
        "--   Chat:send(\"hello\")   World:setHours(18)   Actor:killAura(1)\n"
        "--   Net.send(msgName, json)  -- raw packet crafter\n"
        "--\n"
        "-- EXAMPLES... dropdown loads ready-made scripts.\n"
        "-- Ctrl+Enter runs the script | STOP aborts loops");
    m_scriptEditors.append(ed);
    m_scriptFiles.append(QString());
    m_scriptDirty.append(false);
    connect(ed, &LuaEditor::cursorInfoChanged, this, [this](int line, int col) {
        if (m_luaCursorPosLabel)
            m_luaCursorPosLabel->setText(QString("Ln %1, Col %2").arg(line).arg(col));
    });
    m_luaCodeEditor = ed;
    new LuaHighlighter(ed->document());
    m_luaHighlighter = nullptr;
    editorCardLayout->addWidget(ed, 1);

    { // Editor status row: Ln:Col
        auto* editorStatusRow = new QWidget();
        auto* editorStatusRowLayout = new QHBoxLayout(editorStatusRow);
        editorStatusRowLayout->setContentsMargins(2, 0, 2, 0);
        editorStatusRowLayout->setSpacing(8);
        m_luaCursorPosLabel = new QLabel("Ln 1, Col 1");
        m_luaCursorPosLabel->setObjectName("cursorPosLabel");
        editorStatusRowLayout->addWidget(m_luaCursorPosLabel);
        editorStatusRowLayout->addStretch();
        editorCardLayout->addWidget(editorStatusRow);
    }
    m_luaSplitter->addWidget(editorCard);

    auto* consoleCard = new QWidget();
    consoleCard->setObjectName("cheatCard");
    auto* consoleCardLayout = new QVBoxLayout(consoleCard);
    consoleCardLayout->setContentsMargins(10, 10, 10, 10);
    consoleCardLayout->setSpacing(8);

    auto* consoleHeader = new QWidget();
    auto* consoleHeaderLayout = new QHBoxLayout(consoleHeader);
    consoleHeaderLayout->setContentsMargins(0, 0, 0, 0);
    consoleHeaderLayout->setSpacing(8);
    auto* consoleLabel = new QLabel("CONSOLE");
    consoleLabel->setObjectName("consoleLabel");
    consoleHeaderLayout->addWidget(consoleLabel);
    consoleHeaderLayout->addStretch();

    auto* clearConsoleBtn = new QPushButton("✕");
    clearConsoleBtn->setObjectName("ghostButton");
    clearConsoleBtn->setFixedSize(26, 22);
    clearConsoleBtn->setToolTip("Clear console");
    connect(clearConsoleBtn, &QPushButton::clicked, this, [this]() {
        if (m_luaConsole) m_luaConsole->clear();
    });
    consoleHeaderLayout->addWidget(clearConsoleBtn);
    consoleCardLayout->addWidget(consoleHeader);

    m_luaConsole = new QTextEdit();
    m_luaConsole->setObjectName("luaConsole");
    m_luaConsole->setReadOnly(true);
    m_luaConsole->setPlaceholderText("[zelvex] console  - prints from your scripts appear here, color-coded.");
    consoleCardLayout->addWidget(m_luaConsole, 1);
    m_luaSplitter->addWidget(consoleCard);

    m_luaSplitter->setSizes({330, 220});
    editorCard->setMinimumWidth(190);
    consoleCard->setMinimumWidth(180);

    m_luaSplitter->setStretchFactor(0, 3);
    m_luaSplitter->setStretchFactor(1, 2);
    layout->insertWidget(layout->count() - 1, m_luaSplitter, 1);

    m_luaTabLoaded = true;
}

void MainWindow::refreshLuaAttachState() {
    bool attached = m_mm.isAttached();
    if (m_luaAttachBtn) m_luaAttachBtn->setVisible(!attached);
    if (m_luaDetachBtn) m_luaDetachBtn->setVisible(attached);
    if (m_luaAttachStatusLabel) {
        if (attached) {
            m_luaAttachStatusLabel->setText(QString("Attached · PID %1").arg(m_mm.processId()));
            m_luaAttachStatusLabel->setProperty("ok", true);
        } else {
            m_luaAttachStatusLabel->setText("Not attached");
            m_luaAttachStatusLabel->setProperty("ok", false);
            m_luaAttachStatusLabel->setProperty("idle", true);
        }
        m_luaAttachStatusLabel->style()->unpolish(m_luaAttachStatusLabel);
        m_luaAttachStatusLabel->style()->polish(m_luaAttachStatusLabel);
    }
    if (m_luaStatusLabel) {
        if (attached) {
            m_luaStatusLabel->setText("Ready - inject the VM and press EXEC");
            m_luaStatusLabel->setProperty("ok", true);
        } else {
            m_luaStatusLabel->setText("Not attached - click ATTACH GAME to begin");
            m_luaStatusLabel->setProperty("ok", false);
            m_luaStatusLabel->setProperty("idle", true);
        }
        m_luaStatusLabel->style()->unpolish(m_luaStatusLabel);
        m_luaStatusLabel->style()->polish(m_luaStatusLabel);
    }
    if (m_luaVmStatusLabel) {
        m_luaVmStatusLabel->setText("VM OFFLINE");
        m_luaVmStatusLabel->setProperty("on", false);
        m_luaVmStatusLabel->style()->unpolish(m_luaVmStatusLabel);
        m_luaVmStatusLabel->style()->polish(m_luaVmStatusLabel);
    }
    if (attached) {
        m_luaStateAddr = 0;
        m_luaL_loadstringAddr = 0;
        m_lua_pcallAddr = 0;
    }
}

bool MainWindow::injectLuaDll() {
    if (!m_mm.isAttached()) {
        appendLuaOutput("Not attached to game");
        return false;
    }

    QString dllPath = QCoreApplication::applicationDirPath() + "/lua_dll_new.dll";
    if (!QFile::exists(dllPath)) {
        dllPath = "build/bin/lua_dll_new.dll";
        if (!QFile::exists(dllPath)) {
            // Try the old name as fallback
            dllPath = QCoreApplication::applicationDirPath() + "/lua_dll.dll";
            if (!QFile::exists(dllPath)) {
                appendLuaOutput("DLL not found");
                return false;
            }
        }
    }

    // Initialize shared memory
    if (!m_mm.initSharedMemory()) {
        appendLuaOutput("Failed to init shared memory");
        return false;
    }

    // Step 1: Inject DLL
    if (!m_mm.injectDll(dllPath)) {
        appendLuaOutput("DLL injection failed");
        return false;
    }
    appendLuaOutput("DLL loaded");

    // Step 2: Call exported Initialize function (ordinal 1)
    appendLuaOutput("Calling Initialize...");
    if (!m_mm.callDllFunction(dllPath, 1)) {
        appendLuaOutput("Failed to call Initialize");
        return false;
    }
    appendLuaOutput("Initialize called");

    // Step 3: Wait for ready signal
    int timeout = 100;
    while (timeout-- > 0) {
        if (m_mm.getSharedMemory() && m_mm.getSharedMemory()->done) break;
        Sleep(100);
    }

    if (m_mm.getSharedMemory() && m_mm.getSharedMemory()->done) {
        QString msg = "DLL ready!";
        if (strlen(m_mm.getSharedMemory()->output) > 0) {
            msg += " (";
            msg += QString::fromUtf8(m_mm.getSharedMemory()->output);
            msg += ")";
        }
        appendLuaOutput(msg);
        if (m_luaVmStatusLabel) {
            m_luaVmStatusLabel->setText("VM READY");
            m_luaVmStatusLabel->setProperty("on", true);
            m_luaVmStatusLabel->style()->unpolish(m_luaVmStatusLabel);
            m_luaVmStatusLabel->style()->polish(m_luaVmStatusLabel);
        }
        return true;
    } else {
        appendLuaOutput("No ready signal from DLL");
        return false;
    }
}

void MainWindow::executeLuaViaDll(const QString& code) {
    if (m_luaRunning) {
        appendLuaOutput("Script still running - press STOP first (or let it finish).");
        return;
    }
    if (m_luaConsole) m_luaConsole->clear();
    if (!m_mm.getSharedMemory()) {
        appendLuaOutput("DLL not injected - click 'Inject DLL' first");
        return;
    }

    LuaSharedMemory* shared = m_mm.getSharedMemory();

    // Wait for any previous execution to complete
    int timeout = 100;
    while (timeout-- > 0 && shared->command != 0) {
        Sleep(100);
    }

    // Write code to shared memory
    strncpy_s(shared->code, code.toUtf8().constData(), LuaSharedMemory::MAX_CODE - 1);
    shared->output[0] = '\0';
    shared->error = 0;
    shared->done = 0;
    shared->cancel = 0;
    InterlockedExchange(&shared->outLen, 0);
    m_luaStreamPos = 0;   // streaming console: start reading from scratch

    // Signal DLL to execute
    InterlockedExchange(&shared->command, 1);

    // Async completion polling: the GUI thread never blocks, so long-running
    // scripts (infinite loops with wait()) keep running instead of hitting a
    // 10-second timeout. STOP sets shared->cancel and the DLL aborts the script.
    m_luaRunning = true;
    m_luaRunActive = true;
    if (m_luaStopBtn) {
        bool loop = ScriptHasLoop(code);
        m_luaStopBtn->setVisible(loop);
        if (loop) m_luaStopBtn->setEnabled(true);
    }
    if (m_luaStatusLabel)
        m_luaStatusLabel->setText("Running... (STOP cancels, loops run until done)");
    if (!m_luaExecPollTimer) {
        m_luaExecPollTimer = new QTimer(this);
        connect(m_luaExecPollTimer, &QTimer::timeout, this, &MainWindow::onLuaExecPollTick);
    }
    m_luaExecPollTimer->start(100);
}

void MainWindow::onLuaExecPollTick() {
    LuaSharedMemory* shared = m_mm.getSharedMemory();
    if (!shared) return;

    // Live streaming: append whatever the DLL wrote since our last read.
    // The timer keeps running even when idle so background Events.onTick
    // handlers (which print between runs) stay visible.
    LONG len = shared->outLen;
    if (len > 0 && m_luaStreamPos < len) {
        if (len > LuaSharedMemory::MAX_OUTPUT) len = LuaSharedMemory::MAX_OUTPUT;
        int avail = len - m_luaStreamPos;
        QString chunk = QString::fromUtf8(
            reinterpret_cast<const char*>(shared->output + m_luaStreamPos), avail);
        appendLuaOutput(chunk);
        m_luaStreamPos = len;
    }

    if (!shared->done || !m_luaRunActive) return;
    m_luaRunActive = false;              // finalize once per run
    m_luaRunning = false;
    InterlockedExchange(&shared->cancel, 0);
    if (m_luaStopBtn) {
        m_luaStopBtn->setEnabled(false);
        m_luaStopBtn->setVisible(false);
    }
    QString output = QString::fromUtf8(shared->output).trimmed();
    if (output.isEmpty()) appendLuaOutput("<no output>");
    if (shared->error) {
        appendLuaOutput("Execution completed with errors");
    } else {
        appendLuaOutput("Execution completed successfully");
    }
    if (m_luaStatusLabel) {
        if (m_mm.isAttached()) {
            m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusReady));
            m_luaStatusLabel->setProperty("ok", true);
        } else {
            m_luaStatusLabel->setText("Not attached - click ATTACH GAME to begin");
            m_luaStatusLabel->setProperty("ok", false);
            m_luaStatusLabel->setProperty("idle", true);
        }
        m_luaStatusLabel->style()->unpolish(m_luaStatusLabel);
        m_luaStatusLabel->style()->polish(m_luaStatusLabel);
    }
}

void MainWindow::stopLuaScript() {
    if (!m_luaRunning || !m_mm.getSharedMemory()) return;
    InterlockedExchange(&m_mm.getSharedMemory()->cancel, 1);
    appendLuaOutput("Stop requested - script will abort at the next wait()/loop check...");
}

void MainWindow::startLuaDiagPoll() {
    if (!m_mm.isAttached()) {
        setLuaStatus("Attach to game first", false);
        return;
    }
    stopLuaDiagPoll();
    m_luaDiagPollCount = 0;
    if (!m_luaDiagTimer) {
        m_luaDiagTimer = new QTimer(this);
        connect(m_luaDiagTimer, &QTimer::timeout, this, &MainWindow::onLuaDiagPollTick);
    }
    m_luaDiagTimer->start(2000);
    onLuaDiagPollTick();
    setLuaDiagPollBtnState();
}

void MainWindow::stopLuaDiagPoll() {
    if (m_luaDiagTimer) m_luaDiagTimer->stop();
    setLuaDiagPollBtnState();
}

void MainWindow::onLuaDiagPollTick() {
    if (!m_mm.isAttached()) {
        stopLuaDiagPoll();
        setLuaStatus("Disconnected  - attach again", false);
        return;
    }
    if (m_luaDiagPollCount >= 15) {
        stopLuaDiagPoll();
        setLuaStatus("Poll timed out  - VM not ready after 30s. Try entering a world.", false);
        return;
    }
    m_luaDiagPollCount++;
    m_luaStateAddr = 0;
    quint64 vm = findLuaState();
    if (vm != 0) {
        m_luaStateAddr = vm;
        stopLuaDiagPoll();
        setLuaStatus(QString("VM READY: 0x%1  - you can now execute Lua!").arg(vm, 0, 16), true);
        logLuaDiag("POLL SUCCESS: VM found on attempt " + QString::number(m_luaDiagPollCount));
        return;
    }
    logLuaDiag("POLL attempt " + QString::number(m_luaDiagPollCount) + ": VM not ready yet");
    if (m_luaDiagPollCount >= 15) {
        stopLuaDiagPoll();
        setLuaStatus("VM not ready after 30s  - try joining/entering a world", false);
    }
}

void MainWindow::setLuaDiagPollBtnState() {
    if (!m_luaDiagPollBtn) return;
    if (m_luaDiagTimer && m_luaDiagTimer->isActive()) {
        m_luaDiagPollBtn->setText("Stop Poll");
        m_luaDiagPollBtn->setStyleSheet(
            "QPushButton { background-color: #ff4d4d; color: white; border: none;"
            "border-radius: 6px; font-weight: bold; font-size: 12px; }"
            "QPushButton:hover { background-color: #ef4444; }");
    } else {
        m_luaDiagPollBtn->setText("Poll VM (30s)");
        m_luaDiagPollBtn->setStyleSheet(
            "QPushButton { background-color: #f59e0b; color: #0d0d14; border: none;"
            "border-radius: 6px; font-weight: bold; font-size: 12px; }"
            "QPushButton:hover { background-color: #d97706; }");
    }
}

void MainWindow::dumpLuaExports() {
    if (!m_mm.isAttached()) {
        setLuaStatus("Attach to game first", false);
        return;
    }
    logLuaDiag("=== DUMP EXPORTS ===");
    const QStringList dlls = {"libSandboxEngine.dll", "libSandBoxEngine.dll", "libiworld.dll", "liblua.dll", "lua51.dll"};
    QString summary;
    for (const QString& dll : dlls) {
        if (!m_mm.hasModule(dll)) {
            logLuaDiag(dll + ": NOT LOADED");
            summary += dll + ": NOT LOADED\n";
            continue;
        }
        QStringList exports = m_mm.dumpModuleExports(dll);
        logLuaDiag(dll + ": " + QString::number(exports.size()) + " exports");
        summary += dll + ": " + QString::number(exports.size()) + " exports\n";
        QStringList luaRelated;
        for (const QString& exp : exports) {
            QString lower = exp.toLower();
            if (lower.contains("lua") || lower.contains("scriptvm") || lower.contains("luastate") ||
                lower.contains("getmainl") || lower.contains("luadirector") || lower.contains("luavm")) {
                luaRelated.append(exp);
            }
        }
        logLuaDiag("  Lua-related exports (" + QString::number(luaRelated.size()) + "):");
        for (const QString& exp : luaRelated) {
            logLuaDiag("    " + exp);
        }
        if (!luaRelated.isEmpty()) {
            summary += "  Lua exports:\n";
            for (const QString& exp : luaRelated) {
                summary += "    " + exp + "\n";
            }
        }
    }
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/lua_diag.log";
    QMessageBox::information(this, "Exports Dumped",
        "Log saved to: " + logPath + "\n\nSummary:\n" + summary);
    setLuaStatus("Exports dumped to Desktop", true);
}

quint64 MainWindow::findLuaFunction(const char* name) {
    if (!m_mm.isAttached()) return 0;
    quint64 addr = m_mm.resolveExport("liblua.dll", name);
    if (addr == 0) addr = m_mm.resolveExport("lua51.dll", name);
    // lua_pcall is a macro in Lua 5.1; the actual export may be lua_vpcall or lua_pcallk
    if (addr == 0 && strcmp(name, "lua_pcall") == 0) {
        addr = m_mm.resolveExport("liblua.dll", "lua_vpcall");
        if (addr == 0) addr = m_mm.resolveExport("liblua.dll", "lua_pcallk");
        if (addr == 0) addr = m_mm.resolveExport("lua51.dll", "lua_vpcall");
        if (addr == 0) addr = m_mm.resolveExport("lua51.dll", "lua_pcallk");
    }
    return addr;
}

void MainWindow::setLuaStatus(const QString& text, bool ok) {
    if (!m_luaStatusLabel) return;
    m_luaStatusLabel->setText(text);
    m_luaStatusLabel->setProperty("ok", ok);
    m_luaStatusLabel->style()->unpolish(m_luaStatusLabel);
    m_luaStatusLabel->style()->polish(m_luaStatusLabel);
    appendLuaOutput(QString("[%1] %2").arg(QTime::currentTime().toString("hh:mm:ss")).arg(text));
}

void MainWindow::logLuaDiag(const QString& msg) {
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/lua_diag.log";
    QFile f(logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "  " << msg << "\n";
        f.close();
    }
}

QString MainWindow::luaDiagModuleDump() const {
    QString out;
    QStringList wanted = {"libiworld.dll", "libsandboxengine.dll", "libsandbox.dll",
                          "liblua.dll", "lua51.dll", "libengine.dll", "kernel32.dll"};
    for (const auto& mod : m_mm.listModules()) {
        if (wanted.contains(mod.name.toLower())) {
            out += QString("  %1  base=0x%2  size=0x%3\n")
                       .arg(mod.name).arg(mod.baseAddress, 0, 16).arg(mod.size, 0, 16);
        }
    }
    return out;
}

quint64 MainWindow::findLuaState() {
    logLuaDiag("=== findLuaState() ===");
    if (!m_mm.isAttached()) {
        logLuaDiag("FAIL: not attached");
        return 0;
    }
    logLuaDiag(QString("Target PID=%1 EXE=%2 is32=%3")
                   .arg(m_mm.processId()).arg(m_mm.processName()).arg(m_mm.is32BitTarget()));

    const QStringList engineDlls = {"libSandboxEngineDriver.dll", "libSandboxEngine.dll", "libSandBoxEngine.dll", "libiworld.dll"};

    bool anyEngineLoaded = false;
    for (const QString& dll : engineDlls) {
        if (m_mm.hasModule(dll)) anyEngineLoaded = true;
    }
    if (!anyEngineLoaded) {
        QString modules = m_mm.listModules().isEmpty()
            ? QString(" (module list EMPTY  - EnumProcessModulesEx failed or wrong process!)")
            : "\n" + luaDiagModuleDump();
        logLuaDiag("FAIL: no engine DLL loaded in target process. Loaded modules:" + modules);
        setLuaStatus("No engine DLL in process  - click \"Find Game Process\"", false);
        return 0;
    }

    auto resolve = [&](const QStringList& modules, const QString& symbol) -> quint64 {
        for (const QString& m : modules) {
            quint64 a = m_mm.resolveExport(m, symbol);
            if (a) return a;
        }
        return 0;
    };

    quint64 getCoreLuaDirFn = resolve(engineDlls, "GetCoreLuaDirector@MNSandbox");
    quint64 getLuaStateFn = resolve(engineDlls, "getLuaState@SandboxCoreLuaDirector");
    if (getLuaStateFn == 0) getLuaStateFn = resolve(engineDlls, "GetLuaState@SandboxCoreLuaDirector");
    quint64 getMainLFn = resolve(engineDlls, "GetMainL@LuaLinker");
    quint64 getScriptVmFn = resolve(engineDlls, "game@ScriptVM");
    quint64 getScriptVmStateFn = resolve(engineDlls, "getLuaState@ScriptVM");

    logLuaDiag(QString("exports: GetCoreLuaDirector=0x%1 getLuaState=0x%2 GetMainL=0x%3 ScriptVM::game=0x%4 ScriptVM::getLuaState=0x%5")
                   .arg(getCoreLuaDirFn, 0, 16).arg(getLuaStateFn, 0, 16).arg(getMainLFn, 0, 16)
                   .arg(getScriptVmFn, 0, 16).arg(getScriptVmStateFn, 0, 16));

    QString method;
    QString code;
    if (getCoreLuaDirFn && getLuaStateFn) {
        method = "Director";
        code = QString(
            "call %1\n"
            "test eax,eax\n"
            "je end\n"
            "mov ecx,eax\n"
            "call %2\n"
            "end:\n"
            "ret\n"
        ).arg(getCoreLuaDirFn).arg(getLuaStateFn);
    } else if (getScriptVmFn && getScriptVmStateFn) {
        method = "ScriptVM";
        code = QString(
            "call %1\n"
            "test eax,eax\n"
            "je end\n"
            "mov ecx,eax\n"
            "call %2\n"
            "end:\n"
            "ret\n"
        ).arg(getScriptVmFn).arg(getScriptVmStateFn);
    } else if (getMainLFn) {
        method = "GetMainL";
        code = QString(
            "call %1\n"
            "ret\n"
        ).arg(getMainLFn);
    } else {
        logLuaDiag("FAIL: none of the Lua export symbols resolved in any loaded module");
        setLuaStatus("Exports not found  - engine DLLs present but no Lua exports", false);
        return 0;
    }
    logLuaDiag("method=" + method);

    quint64 remoteShellAddr = m_mm.allocateRemote(128);
    if (remoteShellAddr == 0) {
        logLuaDiag("FAIL: allocateRemote returned 0 (lastError=" + QString::number(GetLastError()) + ")");
        setLuaStatus("Alloc failed in target  - retry", false);
        return 0;
    }

    QMap<QString, quint64> syms;
    QByteArray shellcode = buildShellcode(code, remoteShellAddr, 0, 0, syms);
    if (shellcode.isEmpty()) {
        logLuaDiag("FAIL: buildShellcode empty for:\n" + code);
        m_mm.freeRemote(remoteShellAddr);
        setLuaStatus("Shellcode assembly failed  - internal error", false);
        return 0;
    }
    logLuaDiag("shellcode (" + QString::number(shellcode.size()) + " bytes): " + shellcode.toHex(' '));

    if (!m_mm.writeBytes(remoteShellAddr, shellcode)) {
        logLuaDiag("FAIL: writeBytes failed (lastError=" + QString::number(GetLastError()) + ")");
        m_mm.freeRemote(remoteShellAddr);
        setLuaStatus("Write to target failed  - retry", false);
        return 0;
    }

    HANDLE hThread = CreateRemoteThread(m_mm.handle(), nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellAddr), nullptr, 0, nullptr);
    if (!hThread) {
        logLuaDiag("FAIL: CreateRemoteThread failed (lastError=" + QString::number(GetLastError()) + ")");
        m_mm.freeRemote(remoteShellAddr);
        setLuaStatus("CreateRemoteThread failed", false);
        return 0;
    }
    WaitForSingleObject(hThread, 3000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    m_mm.freeRemote(remoteShellAddr);

    quint64 result = static_cast<quint64>(exitCode);
    logLuaDiag(QString("result=0x%1 [%2]").arg(result, 0, 16).arg(method));
    if (result != 0) {
        setLuaStatus(QString("VM: 0x%1  [%2]").arg(result, 0, 16).arg(method), true);
        logLuaDiag("SUCCESS: lua_State found via " + method);
    } else {
        setLuaStatus(QString("VM returned NULL [%1]  - enter a world/map first, then Recheck").arg(method), false);
        logLuaDiag("FAIL: remote thread returned NULL via " + method + " (director/singleton exists but Lua state not ready)");
    }
    return result;
}

void MainWindow::appendLuaOutput(const QString& text) {
    QString escaped = text.toHtmlEscaped();

    QString color = "#b4b4c8";
    QString lower = text.toLower();
    bool err = text.startsWith("ERR:") || lower.contains("[lua error]") ||
               lower.contains("failed") || lower.contains("null") ||
               lower.contains("not found") || lower.contains("missing");
    bool ok = text.startsWith("OK") || lower.contains("ready") || lower.contains("done") ||
              lower.contains("success") || lower.contains("loaded") || lower.contains("true");
    bool diag = lower.contains("[zelvex]") || lower.contains("native-exec") ||
                lower.contains("vm:") || text.startsWith("T=");

    if (err) color = "#F85149";
    else if (ok) color = "#3FB950";
    else if (diag) color = "#38BDF8";

    appendConsoleHtml(QString("<span style=\"color:%1;\">%2</span>").arg(color, escaped), text);
}

QString MainWindow::wrapLuaCodeWithOutputCapture(const QString& userCode) const {
    QString wrapped =
        "local zelvex_f = io.open('zelvex_output.txt', 'w')\n"
        "local zelvex_orig_print = print\n"
        "print = function(...)\n"
        "  local args = {}\n"
        "  for i = 1, select('#', ...) do args[i] = tostring(select(i, ...)) end\n"
        "  local line = table.concat(args, '\\t')\n"
        "  if zelvex_f then zelvex_f:write(line .. '\\n'):flush() end\n"
        "  zelvex_orig_print(...)\n"
        "end\n"
        "local zelvex_ok, zelvex_err = pcall(function()\n";
    for (const QString& line : userCode.split('\n')) {
        wrapped += "  " + line + "\n";
    }
    wrapped +=
        "end)\n"
        "if not zelvex_ok then\n"
        "  if zelvex_f then zelvex_f:write('ERROR: ' .. tostring(zelvex_err) .. '\\n'):flush() end\n"
        "  zelvex_orig_print('ERROR: ' .. tostring(zelvex_err))\n"
        "end\n"
        "print = zelvex_orig_print\n"
        "if zelvex_f then zelvex_f:close() end\n";
    return wrapped;
}

void MainWindow::readLuaOutputFile() {
    QStringList possiblePaths;
    possiblePaths << "zelvex_output.txt";
    possiblePaths << QDir::currentPath() + "/zelvex_output.txt";
    possiblePaths << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/zelvex_output.txt";
    for (const QString& path : possiblePaths) {
        QFile f(path);
        if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(f.readAll());
            f.close();
            if (!content.isEmpty()) {
                appendLuaOutput(content.trimmed());
                QFile::remove(path);
                return;
            }
        }
    }
}

void MainWindow::executeLuaCode() {
    if (!m_mm.isAttached()) {
        if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).attachToGameFirst));
        return;
    }
    if (!m_luaCodeEditor) return;
    QString code = m_luaCodeEditor->toPlainText().trimmed();
    if (code.isEmpty()) return;

    // DLL injection is required - the shellcode path cannot capture output
    // because the game's Lua sandbox blocks io.open.
    if (!m_mm.getSharedMemory()) {
        appendLuaOutput("Click 'Inject DLL' first, then run the code.");
        return;
    }

    m_luaHistory.append(code);
    if (m_luaHistoryCombo) {
        m_luaHistoryCombo->addItem(code.left(60).replace('\n', ' '));
        if (!m_luaHistoryCombo->isVisible()) m_luaHistoryCombo->setVisible(true);
    }
    executeLuaViaDll(code);
    return;

    // Fallback: shellcode injection (may deadlock on Lua VM)
    QString wrappedCode = wrapLuaCodeWithOutputCapture(code);
    code = wrappedCode;

    if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusExecuting));

    const QStringList engineDlls = {"libSandboxEngine.dll", "libSandBoxEngine.dll", "libiworld.dll", "liblua.dll"};

    auto resolve = [&](const QString& symbol) -> quint64 {
        for (const QString& m : engineDlls) {
            quint64 a = m_mm.resolveExport(m, symbol);
            if (a) return a;
        }
        return 0;
    };

    quint64 getProxyFn = resolve("GetLuaInterfaceProxy");
    quint64 callLuaStringFn = resolve("callLuaString@LuaInterfaceProxy");

    if (getProxyFn == 0 || callLuaStringFn == 0) {
        logLuaDiag("LuaInterfaceProxy approach failed: GetLuaInterfaceProxy=0x" + QString::number(getProxyFn, 16) + " callLuaString=0x" + QString::number(callLuaStringFn, 16));
        setLuaStatus("LuaInterfaceProxy not found  - trying lua_State approach", false);
        if (m_luaStateAddr == 0) {
            m_luaStateAddr = findLuaState();
            if (m_luaStateAddr == 0) {
                return;
            }
        }
        if (m_luaL_loadstringAddr == 0)
            m_luaL_loadstringAddr = findLuaFunction("luaL_loadstring");
        if (m_lua_pcallAddr == 0)
            m_lua_pcallAddr = findLuaFunction("lua_pcall");
        if (m_luaL_loadstringAddr == 0 || m_lua_pcallAddr == 0) {
            if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
            return;
        }
        QByteArray codeUtf8 = code.toUtf8();
        quint64 codeLen = codeUtf8.size() + 1;
        HANDLE hProc = m_mm.handle();
        quint64 remoteCodeAddr = m_mm.allocateRemote(codeLen);
        if (remoteCodeAddr == 0) {
            if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
            return;
        }
        m_mm.writeBytes(remoteCodeAddr, codeUtf8);
        quint64 remoteShellAddr = m_mm.allocateRemote(256);
        if (remoteShellAddr == 0) {
            m_mm.freeRemote(remoteCodeAddr);
            if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
            return;
        }
        quint64 luaL_loadstringFn = m_luaL_loadstringAddr;
        quint64 lua_pcallFn = m_lua_pcallAddr;
        quint64 luaState = m_luaStateAddr;
        QString asmCode = QString(
            "pushad\n"
            "push %1\n"
            "push %2\n"
            "call %3\n"
            "test eax,eax\n"
            "jne fail\n"
            "push 0\n"
            "push -1\n"
            "push 0\n"
            "push %2\n"
            "call %4\n"
            "fail:\n"
            "popad\n"
            "ret\n"
        ).arg(remoteCodeAddr).arg(luaState).arg(luaL_loadstringFn).arg(lua_pcallFn);
        QMap<QString, quint64> syms;
        QByteArray shellcode = buildShellcode(asmCode, remoteShellAddr, 0, 0, syms);
        if (shellcode.isEmpty()) {
            m_mm.freeRemote(remoteCodeAddr);
            m_mm.freeRemote(remoteShellAddr);
            if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
            return;
        }
        m_mm.writeBytes(remoteShellAddr, shellcode);
        HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellAddr), nullptr, 0, nullptr);
        if (hThread) {
            WaitForSingleObject(hThread, 5000);
            DWORD exitCode = 0;
            GetExitCodeThread(hThread, &exitCode);
            CloseHandle(hThread);
            if (exitCode == 0 || exitCode == 1) {
                if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusSuccess));
            } else {
                if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
            }
        } else {
            if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
        }
        m_luaHistory.append(code);
        if (m_luaHistoryCombo) {
            m_luaHistoryCombo->addItem(code.left(60).replace('\n', ' '));
        }
        m_mm.freeRemote(remoteCodeAddr);
        m_mm.freeRemote(remoteShellAddr);
        return;
    }

    logLuaDiag("Using LuaInterfaceProxy approach: GetLuaInterfaceProxy=0x" + QString::number(getProxyFn, 16) + " callLuaString=0x" + QString::number(callLuaStringFn, 16));

    QByteArray codeUtf8 = code.toUtf8();
    quint64 codeLen = codeUtf8.size() + 1;

    HANDLE hProc = m_mm.handle();
    quint64 remoteCodeAddr = m_mm.allocateRemote(codeLen);
    if (remoteCodeAddr == 0) {
        if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
        return;
    }
    m_mm.writeBytes(remoteCodeAddr, codeUtf8);

    quint64 remoteShellAddr = m_mm.allocateRemote(128);
    if (remoteShellAddr == 0) {
        m_mm.freeRemote(remoteCodeAddr);
        if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
        return;
    }

    QString asmCode = QString(
        "call %1\n"
        "mov ecx,eax\n"
        "push %2\n"
        "call %3\n"
        "ret\n"
    ).arg(getProxyFn).arg(remoteCodeAddr).arg(callLuaStringFn);

    QMap<QString, quint64> syms;
    QByteArray shellcode = buildShellcode(asmCode, remoteShellAddr, 0, 0, syms);
    if (shellcode.isEmpty()) {
        logLuaDiag("FAIL: buildShellcode empty for LuaInterfaceProxy approach");
        m_mm.freeRemote(remoteCodeAddr);
        m_mm.freeRemote(remoteShellAddr);
        if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
        return;
    }
    logLuaDiag("LuaInterfaceProxy shellcode (" + QString::number(shellcode.size()) + " bytes): " + shellcode.toHex(' '));

    m_mm.writeBytes(remoteShellAddr, shellcode);

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellAddr), nullptr, 0, nullptr);
    if (hThread) {
        WaitForSingleObject(hThread, 5000);
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);
        CloseHandle(hThread);
        logLuaDiag("LuaInterfaceProxy call returned: " + QString::number(exitCode));
        if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusSuccess));
    } else {
        logLuaDiag("FAIL: CreateRemoteThread failed for LuaInterfaceProxy (lastError=" + QString::number(GetLastError()) + ")");
        if (m_luaStatusLabel) m_luaStatusLabel->setText(QString::fromUtf8(lang(m_currentLanguage).luaStatusError));
    }

    m_luaHistory.append(code);
    if (m_luaHistoryCombo) {
        m_luaHistoryCombo->addItem(code.left(60).replace('\n', ' '));
    }

    m_mm.freeRemote(remoteCodeAddr);
    m_mm.freeRemote(remoteShellAddr);

    // Read output file after a short delay
    QTimer::singleShot(500, this, [this]() { readLuaOutputFile(); });
}

// ── Item Grid ──

void MainWindow::updateItemGrid() {
    if (!m_itemGridLayout) return;

    while (m_itemGridLayout->count() > 0) {
        QLayoutItem* item = m_itemGridLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    const auto& db = m_itemDatabase;
    QString category = m_currentItemCategory;
    QString search = m_itemSearchInput ? m_itemSearchInput->text().toLower() : "";

    QList<ItemDef> filtered;
    for (const auto& cat : db) {
        if (category.isEmpty() || cat.name == category) {
            for (const auto& item : cat.items) {
                if (search.isEmpty() || QString(item.name).toLower().contains(search))
                    filtered.append(item);
            }
        }
    }

    const int COLS = 5;
    const int PER_PAGE = 20;
    int totalPages = qMax(1, (filtered.size() + PER_PAGE - 1) / PER_PAGE);
    if (m_currentItemPage > totalPages) m_currentItemPage = totalPages;

    if (m_itemsPageLabel)
        m_itemsPageLabel->setText(QString("Page %1 / %2").arg(m_currentItemPage).arg(totalPages));

    int start = (m_currentItemPage - 1) * PER_PAGE;
    int end = qMin(start + PER_PAGE, filtered.size());

    for (int i = start; i < end; i++) {
        int row = (i - start) / COLS;
        int col = (i - start) % COLS;
        const auto& item = filtered[i];

        auto* btn = new QToolButton();
        btn->setObjectName("itemButton");
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setIconSize(QSize(44, 44));
        btn->setMinimumSize(8, 74);
        QString iconPath = QString(":/src/images/items/%1.png").arg(item.id);
        QIcon ic(iconPath);
        if (!ic.isNull()) {
            btn->setIcon(ic);
        } else {
            QPixmap pm(44, 44);
            pm.fill(QColor("#242c38"));
            QPainter p(&pm);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#333d4d"));
            p.drawRect(0, 0, 44, 44);
            p.setPen(QColor("#9aa3b2"));
            QFont f = p.font(); f.setPointSize(7); p.setFont(f);
            p.drawText(QRectF(0, 0, 44, 44), Qt::AlignCenter, QString::number(item.id));
            btn->setIcon(QIcon(pm));
        }
        btn->setText(QString("%1[%2]").arg(item.name).arg(item.id));
        btn->setToolTip(QString("%1  (ID %2)").arg(item.name).arg(item.id));
        btn->setStyleSheet(
            "QPushButton#itemButton { background-color: #10141a;"
            "border: 1px solid #1e2733; border-radius: 6px; color: #e6e9ee; font-size: 9px;"
            "font-weight: 600; padding: 4px; text-align: center; }"
            "QToolButton#itemButton:hover { background-color: #151a22;"
            "border-color: rgba(61,139,255,0.4); }");
        connect(btn, &QPushButton::clicked, this, [this, item]() {
            int qty = 1;
            if (m_itemQtyInput) {
                bool ok = false;
                int v = m_itemQtyInput->text().toInt(&ok);
                if (ok && v > 0 && v <= 999) qty = v;
            }
            giveItem(item.id, qty);
        });
        m_itemGridLayout->addWidget(btn, row, col);
    }
}

// ── Build Shellcode (two-pass) ──

QByteArray MainWindow::buildShellcode(const QString& newmemCode, quint64 remoteBase, quint64 returnAddr,
                                       quint64 globalAllocAddr, const QMap<QString, quint64>& globalSymbols) {
    if (!m_assembler.isOpen())
        m_assembler.open(KS_ARCH_X86, KS_MODE_32);

    QStringList lines = newmemCode.split('\n', Qt::SkipEmptyParts);
    QMap<QString, int> labelPos;
    QList<QByteArray> assembledLines;
    QList<int> lineTypes;

    int pos = 0;
    for (int idx = 0; idx < lines.size(); idx++) {
        QString trimmed = lines[idx].trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed.endsWith(':') && !trimmed.contains(' ')) {
            labelPos[trimmed.chopped(1)] = pos;
            lineTypes.append(1);
            assembledLines.append(QByteArray());
            continue;
        }

        if (trimmed.startsWith("jmp ") && trimmed.contains("return")) {
            lineTypes.append(2);
            assembledLines.append(QByteArray(5, '\x00'));
            pos += 5;
            continue;
        }

        bool isInternalJump = (trimmed.startsWith("je ") || trimmed.startsWith("jne ") || trimmed.startsWith("jmp "))
                              && !trimmed.contains("return");
        if (isInternalJump) {
            int jumpSize = (trimmed.startsWith("je ") || trimmed.startsWith("jne ")) ? 6 : 5;
            lineTypes.append(3);
            assembledLines.append(QByteArray(jumpSize, '\x00'));
            pos += jumpSize;
            continue;
        }

        if (trimmed.endsWith(':')) {
            lineTypes.append(1);
            assembledLines.append(QByteArray());
            continue;
        }

        QByteArray assembled = assembleOneLine(m_assembler, trimmed, remoteBase + pos, globalSymbols, globalAllocAddr);
        if (assembled.isEmpty() && !trimmed.startsWith("//")) {
            return {};
        }
        lineTypes.append(0);
        assembledLines.append(assembled);
        pos += assembled.size();
    }

    // Pass 2: fill jump offsets
    pos = 0;
    int lineIdx = 0;
    for (int idx = 0; idx < lines.size(); idx++) {
        QString trimmed = lines[idx].trimmed();
        if (trimmed.isEmpty()) continue;

        int lt = lineTypes[lineIdx];
        QByteArray& assembled = assembledLines[lineIdx];
        lineIdx++;

        if (lt == 1) continue;

        if (lt == 2) {
            assembled[0] = '\xE9';
            qint32 rel32 = static_cast<qint32>(returnAddr) - static_cast<qint32>(remoteBase + pos + 5);
            memcpy(assembled.data() + 1, &rel32, 4);
            pos += 5;
            continue;
        }

        if (lt == 3) {
            QString targetLabel = trimmed.section(' ', 1).trimmed();
            if (labelPos.contains(targetLabel)) {
                int target = labelPos[targetLabel];
                if (trimmed.startsWith("je ")) {
                    assembled[0] = '\x0F'; assembled[1] = '\x84';
                    qint32 off = target - (pos + 6);
                    memcpy(assembled.data() + 2, &off, 4);
                } else if (trimmed.startsWith("jne ")) {
                    assembled[0] = '\x0F'; assembled[1] = '\x85';
                    qint32 off = target - (pos + 6);
                    memcpy(assembled.data() + 2, &off, 4);
                } else {
                    assembled[0] = '\xE9';
                    qint32 off = target - (pos + 5);
                    memcpy(assembled.data() + 1, &off, 4);
                }
            }
            pos += assembled.size();
            continue;
        }

        pos += assembled.size();
    }

    QByteArray shellcode;
    for (const QByteArray& a : assembledLines)
        shellcode.append(a);
    return shellcode;
}

// ── Assemble One Line ──

QByteArray MainWindow::assembleOneLine(Assembler& asm32, const QString& line, quint64 address,
                                        const QMap<QString, quint64>& globalSymbols, quint64 globalAllocAddr) {
    QString trimmed = line.trimmed();

    if (trimmed.startsWith("retn")) trimmed = "ret";

    // Handle (float) instructions
    if (trimmed.contains("(float)")) {
        static const QRegularExpression floatRe(R"(mov\s+(?:dword\s+ptr\s+)?\[(\w+)(?:\+([\da-fA-F]+h?))?\]\s*,\s*\(float\)([-\w.]+))");
        QRegularExpressionMatch fm = floatRe.match(trimmed);
        QByteArray result;
        if (fm.hasMatch()) {
            float fval = fm.captured(3).toFloat();
            quint32 bits = Assembler::floatToUint32(fval);
            QString reg = fm.captured(1);
            QString offsetStr = fm.captured(2);
            if (reg == "eax" && offsetStr.isEmpty()) {
                result.append('\xC7'); result.append('\x00');
            } else if (reg == "eax") {
                quint8 off = static_cast<quint8>(offsetStr.toUInt(nullptr, 16));
                result.append('\xC7'); result.append('\x40'); result.append(static_cast<char>(off));
            } else {
                result.append('\xC7'); result.append('\x05');
                quint32 absAddr = static_cast<quint32>(globalAllocAddr);
                if (offsetStr.isEmpty()) absAddr += (reg == "eax") ? 0 : 4;
                result.append(reinterpret_cast<const char*>(&absAddr), 4);
            }
            result.append(reinterpret_cast<const char*>(&bits), 4);
            return result;
        }
        static const QRegularExpression floatRe2(R"(mov\s+(dword\s+ptr\s+)?\[([a-z]+)(?:\+0x([0-9a-fA-F]+))?\]\s*,\s*\(float\)([-\w.]+))");
        QRegularExpressionMatch fm2 = floatRe2.match(trimmed);
        if (fm2.hasMatch()) {
            float fval = fm2.captured(4).toFloat();
            quint32 bits = Assembler::floatToUint32(fval);
            QString reg = fm2.captured(2);
            quint32 off = fm2.captured(3).toUInt(nullptr, 16);
            if (reg == "eax" && off == 0) { result.append('\xC7'); result.append('\x00'); }
            else if (reg == "eax") { result.append('\xC7'); result.append('\x40'); result.append(static_cast<char>(off)); }
            else if (reg == "ecx") { result.append('\xC7'); result.append('\x41'); result.append(static_cast<char>(off)); }
            result.append(reinterpret_cast<const char*>(&bits), 4);
            return result;
        }
    }

    // Symbol replacement  - sort by length descending to prevent "string" from corrupting "string1"
    QList<QString> symKeys = globalSymbols.keys();
    std::sort(symKeys.begin(), symKeys.end(), [](const QString& a, const QString& b) {
        return a.length() > b.length();
    });
    for (const QString& key : symKeys) {
        trimmed.replace(key, "0x" + QString::number(globalSymbols[key], 16));
    }

    // Resolve libSandboxEngine symbols
    static const QRegularExpression sandboxRe(R"(libSandboxEngine\.([\w:]+))");
    QRegularExpressionMatchIterator sbIt = sandboxRe.globalMatch(trimmed);
    while (sbIt.hasNext()) {
        QRegularExpressionMatch sm = sbIt.next();
        QString fullName = sm.captured(0);
        QString funcName = sm.captured(1);
        int lastColon = funcName.lastIndexOf("::");
        QString shortName = (lastColon >= 0) ? funcName.mid(lastColon + 2) : funcName;
        quint64 addr = m_mm.resolveExport("libSandboxEngine.dll", shortName);
        if (addr == 0) addr = m_mm.resolveExport("libSandboxEngine.dll", funcName);
        if (addr == 0) addr = m_mm.resolvePdbSymbol("libSandboxEngine.dll", funcName);
        if (addr != 0) trimmed.replace(fullName, "0x" + QString::number(addr, 16));
    }

    // Resolve libSandboxEngine.dll+offset
    static const QRegularExpression sandboxDllRe(R"(libSandboxEngine\.dll\+([0-9a-fA-F]+))");
    QRegularExpressionMatch sdlIt = sandboxDllRe.match(trimmed);
    if (sdlIt.hasMatch()) {
        quint64 addr = m_mm.parseAddress(sdlIt.captured(0));
        if (addr != 0) trimmed.replace(sdlIt.captured(0), "0x" + QString::number(addr, 16));
    }

    // Resolve libiworld symbols (handles +offset suffix)
    static const QRegularExpression iworldRe(R"(libiworld\.([\w:]+)(?:\+([0-9a-fA-F]+))?)");
    QRegularExpressionMatchIterator iwIt = iworldRe.globalMatch(trimmed);
    while (iwIt.hasNext()) {
        QRegularExpressionMatch iwm = iwIt.next();
        QString fullName = iwm.captured(0);
        QString funcName = iwm.captured(1);
        QString offsetSuffix = iwm.captured(2);
        int lastColon = funcName.lastIndexOf("::");
        QString shortName = (lastColon >= 0) ? funcName.mid(lastColon + 2) : funcName;
        quint64 addr = m_mm.resolveExport("libiworld.dll", shortName);
        if (addr == 0) addr = m_mm.resolveExport("libiworld.dll", funcName);
        if (addr == 0) addr = m_mm.resolvePdbSymbol("libiworld.dll", funcName);
        if (addr != 0) {
            if (!offsetSuffix.isEmpty())
                addr += offsetSuffix.toULongLong(nullptr, 16);
            trimmed.replace(fullName, "0x" + QString::number(addr, 16));
        }
    }

    // Resolve libSandboxEngineDriver symbols (handles +offset suffix)
    static const QRegularExpression driverRe(R"(libSandboxEngineDriver\.([\w:]+)(?:\+([0-9a-fA-F]+))?)");
    QRegularExpressionMatchIterator drIt = driverRe.globalMatch(trimmed);
    while (drIt.hasNext()) {
        QRegularExpressionMatch drm = drIt.next();
        QString fullName = drm.captured(0);
        QString funcName = drm.captured(1);
        QString offsetSuffix = drm.captured(2);
        int lastColon = funcName.lastIndexOf("::");
        QString shortName = (lastColon >= 0) ? funcName.mid(lastColon + 2) : funcName;
        quint64 addr = m_mm.resolveExport("libSandboxEngineDriver.dll", shortName);
        if (addr == 0) addr = m_mm.resolveExport("libSandboxEngineDriver.dll", funcName);
        if (addr == 0) addr = m_mm.resolvePdbSymbol("libSandboxEngineDriver.dll", funcName);
        if (addr != 0) {
            if (!offsetSuffix.isEmpty())
                addr += offsetSuffix.toULongLong(nullptr, 16);
            trimmed.replace(fullName, "0x" + QString::number(addr, 16));
        }
    }

    // Resolve kernel32.sleep
    if (trimmed.contains("kernel32.sleep", Qt::CaseInsensitive)) {
        quint64 addr = m_mm.resolveExport("kernel32.dll", "Sleep");
        if (addr != 0) {
            QString hexAddr = "0x" + QString::number(addr, 16);
            trimmed.replace("call kernel32.sleep", "call " + hexAddr);
            trimmed.replace("call Kernel32.sleep", "call " + hexAddr);
        }
    }

    // Replace # with nothing
    trimmed.replace("#", "");

    // Try Keystone
    QString err;
    QByteArray assembled = asm32.assemble(trimmed, address, &err);
    if (!assembled.isEmpty()) return assembled;

    // ── Fallback patterns ──

    // push dword ptr [reg]
    static const QRegularExpression pushRegRe(R"(push\s+(?:dword\s+ptr\s+)?\[(\w+)\])");
    QRegularExpressionMatch prm = pushRegRe.match(trimmed);
    if (prm.hasMatch()) {
        QString reg = prm.captured(1);
        static const QMap<QString, quint8> regMap = {
            {"eax", 0x00}, {"ecx", 0x01}, {"edx", 0x02}, {"ebx", 0x03},
            {"esp", 0x04}, {"ebp", 0x05}, {"esi", 0x06}, {"edi", 0x07}
        };
        if (regMap.contains(reg)) {
            QByteArray r;
            r.append('\xFF');
            r.append(static_cast<char>(0x30 | regMap[reg]));
            return r;
        }
    }

    // push dword ptr [addr32]
    static const QRegularExpression pushAddrRe(R"(push\s+(?:dword\s+ptr\s+)?\[([\da-fA-Fx]+)\])");
    QRegularExpressionMatch pam = pushAddrRe.match(trimmed);
    if (pam.hasMatch()) {
        quint32 addr = pam.captured(1).toUInt(nullptr, 16);
        QByteArray r;
        r.append('\xFF'); r.append('\x35');
        r.append(reinterpret_cast<const char*>(&addr), 4);
        return r;
    }

    // mov reg,[addr32]
    static const QRegularExpression movRegAddrRe(R"(mov\s+(\w+),\s*(?:dword\s+ptr\s+)?\[([\da-fA-Fx]+)\])");
    QRegularExpressionMatch mra = movRegAddrRe.match(trimmed);
    if (mra.hasMatch()) {
        QString reg = mra.captured(1);
        quint32 addr = mra.captured(2).toUInt(nullptr, 16);
        static const QMap<QString, quint8> regOpMap = {
            {"eax", 0x00}, {"ecx", 0x01}, {"edx", 0x02}, {"ebx", 0x03},
            {"esp", 0x04}, {"ebp", 0x05}, {"esi", 0x06}, {"edi", 0x07}
        };
        if (regOpMap.contains(reg)) {
            QByteArray r;
            r.append('\x8B');
            r.append(static_cast<char>(0x08 | regOpMap[reg]));
            r.append(reinterpret_cast<const char*>(&addr), 4);
            return r;
        }
    }

    // cmp reg,[addr32]
    static const QRegularExpression cmpRegAddrRe(R"(cmp\s+(\w+),\s*(?:dword\s+ptr\s+)?\[([\da-fA-Fx]+)\])");
    QRegularExpressionMatch cra = cmpRegAddrRe.match(trimmed);
    if (cra.hasMatch()) {
        QString reg = cra.captured(1);
        quint32 addr = cra.captured(2).toUInt(nullptr, 16);
        static const QMap<QString, quint8> regCmpMap = {
            {"eax", 0x00}, {"ecx", 0x01}, {"edx", 0x02}, {"ebx", 0x03},
            {"esp", 0x04}, {"ebp", 0x05}, {"esi", 0x06}, {"edi", 0x07}
        };
        if (regCmpMap.contains(reg)) {
            QByteArray r;
            r.append('\x3B');
            r.append(static_cast<char>(0x08 | regCmpMap[reg]));
            r.append(reinterpret_cast<const char*>(&addr), 4);
            return r;
        }
    }

    // call rel32  - handle `call 0xADDR` where Keystone fails on 64-bit addresses
    static const QRegularExpression callAddrRe(R"(call\s+([\da-fA-Fx]+))");
    QRegularExpressionMatch ca = callAddrRe.match(trimmed);
    if (ca.hasMatch()) {
        quint32 target = ca.captured(1).toUInt(nullptr, 16);
        quint32 rel32 = target - static_cast<quint32>(address + 5);
        QByteArray r;
        r.append('\xE8');
        r.append(reinterpret_cast<const char*>(&rel32), 4);
        return r;
    }

    // Fallback: cmp dword ptr [addr], imm
    static const QRegularExpression cmpRe(R"(cmp\s+(?:dword\s+ptr\s+)?\[([\da-fA-Fx]+)\]\s*,\s*(\d+))");
    QRegularExpressionMatch cm = cmpRe.match(trimmed);
    if (cm.hasMatch()) {
        QByteArray r;
        quint32 addr = cm.captured(1).toUInt(nullptr, 16);
        quint32 imm = cm.captured(2).toUInt();
        r.append('\x83'); r.append('\x3D');
        r.append(reinterpret_cast<const char*>(&addr), 4);
        r.append(static_cast<char>(imm));
        return r;
    }

    // Fallback: mov dword ptr [addr], imm
    static const QRegularExpression movRe(R"(mov\s+(?:dword\s+ptr\s+)?\[([\da-fA-Fx]+)\]\s*,\s*(\d+))");
    QRegularExpressionMatch mm = movRe.match(trimmed);
    if (mm.hasMatch()) {
        QByteArray r;
        quint32 addr = mm.captured(1).toUInt(nullptr, 16);
        quint32 imm = mm.captured(2).toUInt();
        r.append('\xC7'); r.append('\x05');
        r.append(reinterpret_cast<const char*>(&addr), 4);
        quint32 imm32 = imm;
        r.append(reinterpret_cast<const char*>(&imm32), 4);
        return r;
    }

    return {};
}

#include "mainwindow.moc"

