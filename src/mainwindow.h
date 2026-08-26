#pragma once

#include <QMainWindow>
#include <QShortcut>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QSplitter>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QStatusBar>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QEvent>
#include <QEnterEvent>
#include <QParallelAnimationGroup>
#include <QSettings>
#include <QHash>
#include <QGraphicsOpacityEffect>

class LuaEditor;

#include "memory_manager.h"
#include "assembler.h"
#include "cheat_defs.h"
#include "item_database.h"
#include "loading_screen.h"
#include "translations.h"
#include "lua_highlighter.h"
#include "layout_config.h"

class SwitchButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(float hoverProgress READ hoverProgress WRITE setHoverProgress)
public:
    explicit SwitchButton(QWidget* parent = nullptr) : QAbstractButton(parent) {
        setFixedSize(40, 22);
        setCursor(Qt::PointingHandCursor);
        setCheckable(true);
    }
    float hoverProgress() const { return m_hover; }
    void setHoverProgress(float v) { m_hover = v; update(); }
protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool on = isChecked();
        // Executor-style switch: flat track, square knob, no pill glow.
        QColor trackColor = on ? QColor("#3D8BFF") : QColor("#1a1f27");
        QColor borderColor = on ? QColor("#5AA3FF") : QColor("#2a3441");
        p.setPen(QPen(borderColor, 1));
        p.setBrush(trackColor);
        p.drawRoundedRect(rect(), height() / 2, height() / 2);
        const int knobSize = height() - 8;
        const int knobY = (height() - knobSize) / 2;
        const int knobX = on ? (width() - knobSize - 4) : 4;
        p.setBrush(QColor("#FFFFFF"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(knobX, knobY, knobSize, knobSize);
        if (on) {
            // small check inside the knob area for extra clarity
            p.setPen(QPen(QColor("#3D8BFF"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPointF pts[3] = {
                QPointF(knobX + 3.5, knobY + knobSize * 0.5),
                QPointF(knobX + knobSize * 0.45, knobY + knobSize - 3.0),
                QPointF(knobX + knobSize - 2.0, knobY + 3.0)
            };
            p.drawPolyline(pts, 3);
        }
    }
    void enterEvent(QEnterEvent*) override { m_hover = 1.0; update(); }
    void leaveEvent(QEvent*) override { m_hover = 0.0; update(); }
private:
    float m_hover = 0.0f;
};

struct ToggleItem {
    const CheatDef* cheat = nullptr;
    SwitchButton* pill = nullptr;
};

struct ActionItem {
    const CheatDef* cheat = nullptr;
    QPushButton* button = nullptr;
};

struct InputItem {
    const CheatDef* cheat = nullptr;
    QLineEdit* input = nullptr;
};

struct InjectionRecord {
    quint64 patchAddress = 0;
    quint64 remoteCodeAddress = 0;
    int remoteCodeSize = 0;
    QByteArray originalBytes;
    int patchSize = 0;
    QByteArray expectedBytes;
    HANDLE remoteThread = nullptr;
    quint64 remoteDataAddress = 0;
    quint64 remoteCmdAddress = 0;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void registerHotkeys();
    enum { HOTKEY_ID_1 = 1, HOTKEY_ID_2 = 2, HOTKEY_ID_3 = 3 };

private slots:
    void onAttachClicked(bool isStartup = false);
    void onTabClicked();
    void onTogglePillClicked();
    void toggleSidebar();
    void refreshTimer();
    void checkProcessHealth();
    void onSaveTeleportClicked();
    void onRandomTpTick();

private:
    void setupUI();
    void loadEntries(int onlyTab = -1);
    void switchTab(int index);
    void initLoadingStages();
    void syncTabStates(int tab);
    void setTabIconStates(int index);
    void updateSidebarBody(bool expanded);
    void animatePageChange(int index);
    void appendConsoleHtml(const QString& html, const QString& textPlain = QString());
    void updateStatusVisual(bool attached);

    void applyCheat(const CheatDef& cheat);
    void restoreCheat(const CheatDef& cheat);
    void applyAobDbPatch(const CheatDef& cheat);
    void applyAobDbAction(const CheatDef& cheat);
    void applyAobJmpInject(const CheatDef& cheat);
    void applyAobJmpAction(const CheatDef& cheat);
    void applyTerrainEditor(const CheatDef& cheat);
    void applyGiveItem();
    void applyReadme();
    bool writePointerValue(const CheatDef& cheat, const QString& valueStr);

    void loadSavedTeleports();
    void saveTeleport(const QString& name);
    void deleteTeleport(int index);
    void teleportToSaved(int index);
    void startRandomTeleport();
    void stopRandomTeleport();
    void refreshTeleportList();
    bool isValidTeleportValue(float val) const;

    void updateItemGrid();
    void giveItem(int itemId, int quantity);

    void loadLuaTab();
    void executeLuaCode();
    quint64 findLuaState();
    quint64 findLuaFunction(const char* name);
    void recheckLuaVm();
    void findLuaGameProcess();
    void logLuaDiag(const QString& msg);
    QString luaDiagModuleDump() const;
    void setLuaStatus(const QString& text, bool ok = true);

    QByteArray parseAobPattern(const QString& patternStr, QByteArray& mask) const;
    QByteArray buildShellcode(const QString& newmemCode, quint64 remoteBase, quint64 returnAddr,
                              quint64 globalAllocAddr = 0, const QMap<QString, quint64>& globalSymbols = {});
    QByteArray assembleOneLine(Assembler& asm32, const QString& line, quint64 address,
                               const QMap<QString, quint64>& globalSymbols, quint64 globalAllocAddr);

    QWidget* createToggleRow(const CheatDef* cheat, const QString& subtitle = QString());
    QWidget* createActionRow(const CheatDef* cheat);
    QWidget* createDisabledRow(const CheatDef* cheat);
    QWidget* createInputRow(const CheatDef* cheat);
    QWidget* createComboRow(const QString& label, QComboBox* combo);
    QWidget* createPageHeader(const QString& title);

    const CheatDef* findCheatById(int id);
    void detectStalePatches();
    void preScanAobs();

    QWidget* m_sidebar;
    QList<QPushButton*> m_tabs;
    QList<QLabel*> m_tabIcons;
    QList<QLabel*> m_tabLabels;
    void updateTabIcon(int index);
    int m_activeTab = 0;
    QStackedWidget* m_stack;
    QHash<int, QGraphicsOpacityEffect*> m_pageEffects;
    QLabel* m_statusLabel;
    QLabel* m_statusDot;
    QPushButton* m_attachBtn;
    QPushButton* m_generalAttachBtn = nullptr;
    void updateGeneralAttachBtn();
    QPushButton* m_sidebarToggleBtn;
    QLabel* m_sidebarHeader;
    QLabel* m_sidebarSub;
    QLabel* m_headerPidLabel = nullptr;
    bool m_sidebarCollapsed = true;
    QParallelAnimationGroup* m_sidebarAnim = nullptr;
    LoadingScreen* m_loadingScreen = nullptr;
    LoadingScreen* m_loader = nullptr;
    int m_loadStep = 0;

    MemoryManager m_mm;
    Assembler m_assembler;
    QTimer* m_refreshTimer;
    QTimer* m_healthTimer;

    QList<CheatDef> m_cheats;
    QComboBox* m_conditionCombo = nullptr;

    QList<ToggleItem> m_toggles;
    QList<ActionItem> m_actions;
    QList<InputItem> m_inputs;

    QMap<int, InjectionRecord> m_injections;
    HWND m_gameWindow = nullptr;

    QPoint m_dragPosition;
    bool m_dragging = false;

    // Teleport manager
    QList<bool> m_tabLoaded;
    QLineEdit* m_tpNameInput = nullptr;
    QWidget* m_tpListWidget = nullptr;
    QVBoxLayout* m_tpListLayout = nullptr;
    QList<QPair<QString, QList<float>>> m_savedTeleports;
    QTimer* m_randomTpTimer = nullptr;
    float m_randomTpSavedPos[3] = {0, 0, 0};
    bool m_randomTpActive = false;

    // Items manager
    QList<ItemCategory> m_itemDatabase;
    QMap<int, quint64> m_resolvedAddrCache;  // cheatId -> resolved address
    QMap<int, int> m_resolveFailCount;       // cheatId -> consecutive failures
    bool m_itemsTabLoaded = false;
    bool m_giveItemBusy = false;

    // Lua injector
    bool m_luaTabLoaded = false;
    LuaEditor* m_luaCodeEditor = nullptr;
    QTextEdit* m_luaConsole = nullptr;
    QSplitter* m_luaSplitter = nullptr;
    bool m_consoleAutoScroll = true;
    LuaHighlighter* m_luaHighlighter = nullptr;
    QLabel* m_luaStatusLabel = nullptr;
    QPushButton* m_luaAttachBtn = nullptr;
    QPushButton* m_luaDetachBtn = nullptr;
    QLabel* m_luaAttachStatusLabel = nullptr;
    QLabel* m_luaVmStatusLabel = nullptr;
    QLabel* m_luaCursorPosLabel = nullptr;
    QShortcut* m_luaExecShortcut = nullptr;
    void refreshLuaAttachState();
    bool injectLuaDll();
    void executeLuaViaDll(const QString& code);
    void onLuaExecPollTick();
    void stopLuaScript();
    void appendLuaOutput(const QString& text);
    QString wrapLuaCodeWithOutputCapture(const QString& userCode) const;
    void readLuaOutputFile();
    void startLuaDiagPoll();
    void stopLuaDiagPoll();
    void onLuaDiagPollTick();
    void setLuaDiagPollBtnState();
    void dumpLuaExports();
    QPushButton* m_luaFindGameBtn = nullptr;
    QPushButton* m_luaRecheckBtn = nullptr;
    QPushButton* m_luaDiagPollBtn = nullptr;
    QPushButton* m_luaDumpExportsBtn = nullptr;
    QComboBox* m_luaHistoryCombo = nullptr;
    QList<QString> m_luaHistory;
    // Script tabs / async execution
    QTabWidget* m_scriptTabs = nullptr;
    QVector<LuaEditor*> m_scriptEditors;
    QStringList m_scriptFiles;
    QVector<bool> m_scriptDirty;
    QPushButton* m_luaStopBtn = nullptr;
    QComboBox* m_luaExampleCombo = nullptr;
    QTimer* m_luaExecPollTimer = nullptr;
    bool m_luaRunning = false;
    bool m_luaRunActive = false;   // true while a run is pending/active
    int m_luaStreamPos = 0;        // streaming console read cursor (bytes consumed)
    quint64 m_luaStateAddr = 0;
    quint64 m_luaL_loadstringAddr = 0;
    quint64 m_lua_pcallAddr = 0;
    QTimer* m_luaDiagTimer = nullptr;
    int m_luaDiagPollCount = 0;
    QComboBox* m_itemCategoryCombo = nullptr;
    QLineEdit* m_itemSearchInput = nullptr;
    QLineEdit* m_itemIdInput = nullptr;
    QLineEdit* m_itemQtyInput = nullptr;
    QLineEdit* m_hotkeyEdits[3] = { nullptr, nullptr, nullptr };
    bool m_hotkeysRegistered = false;
    QWidget* m_itemGridWidget = nullptr;
    QGridLayout* m_itemGridLayout = nullptr;
    QLabel* m_itemsPageLabel = nullptr;
    QString m_currentItemCategory;
    int m_currentItemPage = 1;
    void loadItemsTab();

    static const int SIDEBAR_EXPANDED = 220;
    static const int SIDEBAR_COLLAPSED = 64;

    // Language support
    Language m_currentLanguage = Language::English;
    void setLanguage(Language newLang);
    QString trTab(const QString& english) const;
    QString trSection(const QString& english) const;
    QString trCheatName(int cheatId) const;
    QString trCheatDesc(int cheatId) const;
    const char* trUI(const char* key) const;
    QPushButton* m_langButtons[7] = {};
    QLabel* m_statusLabelRef = nullptr;
};

