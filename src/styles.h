#pragma once

#include <QString>

namespace ZelvexStyles {

// Zelvex dark theme  - flat black/white/blue, professional executor look
// (inspired by Synapse X / Wave / Xeno class Roblox executors).
// Palette: near-black surfaces, 1px hairline borders, blue accent (#3D8BFF),
// white primary text, muted secondary text.
inline QString darkTheme() {
    return R"(
        * {
            font-family: "Segoe UI Variable Text", "Segoe UI", "Inter", sans-serif;
            outline: none;
        }
        QMainWindow, QWidget#centralWidget {
            background-color: #000000;
        }

        /* ── Header ── */
        QWidget#headerWidget {
            background-color: #000000;
            border-bottom: 1px solid #1b212b;
        }
        QPushButton#windowBtn, QPushButton#windowBtnClose {
            background-color: transparent;
            color: #7b8494;
            border: none;
            border-radius: 5px;
            font-size: 11px;
            font-weight: 600;
            min-width: 22px;
            min-height: 22px;
        }
        QPushButton#windowBtn:hover {
            background-color: #1a202a;
            color: #e6e9ee;
        }
        QPushButton#windowBtnClose:hover {
            background-color: #c42b1c;
            color: #ffffff;
        }
        QPushButton#sidebarToggleBtn {
            background-color: transparent;
            color: #7b8494;
            border: none;
            border-radius: 6px;
            font-size: 15px;
            min-width: 34px;
            min-height: 34px;
        }
        QPushButton#sidebarToggleBtn:hover {
            background-color: rgba(61,139,255,0.12);
            color: #5aa3ff;
        }

        /* ── Sidebar ── */
        QWidget#sidebar {
            background-color: #000000;
            border-right: 1px solid #1b212b;
        }
        QWidget#sidebarHeaderBox {
            background: transparent;
        }
        QLabel#sidebarTitle {
            font-size: 11px;
            font-weight: 800;
            color: #5aa3ff;
            padding: 6px 16px 0px 16px;
            letter-spacing: 2px;
        }
        QLabel#sidebarSubtitle {
            font-size: 9px;
            color: #525a68;
            padding: 0px 16px 10px 16px;
            font-weight: 500;
        }
        QWidget#tabContainer {
            background-color: transparent;
            border: none;
            border-radius: 7px;
            margin: 1px 8px;
        }
        QWidget#tabContainer:hover {
            background-color: rgba(255,255,255,0.045);
        }
        QWidget#tabContainer[active="true"] {
            background-color: rgba(61,139,255,0.14);
            border: 1px solid rgba(61,139,255,0.25);
        }
        QLabel#tabIcon {
            background-color: transparent;
            border: none;
            border-radius: 6px;
        }
        QLabel#tabText {
            font-size: 13px;
            font-weight: 500;
            color: #8b93a3;
            background-color: transparent;
            border: none;
        }
        QLabel#tabText[active="true"] {
            color: #ffffff;
            font-weight: 600;
        }
        QFrame#sidebarSep {
            background-color: #1b212b;
            max-height: 1px;
            margin: 6px 12px;
            border: none;
        }
        QLabel#statusDot {
            border-radius: 4px;
            border: none;
        }
        QLabel#statusLabel {
            font-size: 11px;
            font-weight: 600;
            color: #7b8494;
            font-family: "Cascadia Code", "Consolas", monospace;
            background-color: transparent;
            border: none;
        }
        QLabel#statusLabel[attached="true"] {
            color: #3fb950;
        }

        /* ── Buttons ── */
        QPushButton#attachButton {
            background-color: #3d8bff;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 6px 16px;
            min-height: 32px;
            min-width: 60px;
            font-size: 12px;
            font-weight: 700;
            letter-spacing: 0.3px;
        }
        QPushButton#attachButton:hover {
            background-color: #5aa3ff;
        }
        QPushButton#attachButton:pressed {
            background-color: #2f6fd8;
        }
        QPushButton#attachButton:disabled {
            background-color: #171b22;
            color: #5f6875;
        }
        QPushButton#attachButton[attached="false"] {
            background-color: #11151b;
            color: #8b93a3;
            border: 1px solid #232b38;
        }

        QPushButton#actionButton {
            background-color: #11151b;
            color: #a6adb9;
            border: 1px solid #232b38;
            border-radius: 6px;
            padding: 6px 14px;
            min-height: 30px;
            min-width: 50px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton#actionButton:hover {
            background-color: #171d26;
            border-color: rgba(61,139,255,0.45);
            color: #ffffff;
        }
        QPushButton#actionButton:pressed {
            background-color: #0e1116;
        }

        QPushButton#primaryButton {
            background-color: #3d8bff;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 6px 16px;
            min-height: 30px;
            min-width: 70px;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#primaryButton:hover {
            background-color: #5aa3ff;
        }
        QPushButton#primaryButton:pressed {
            background-color: #2f6fd8;
        }
        QPushButton#primaryButton:disabled {
            background-color: #171b22;
            color: #5f6875;
        }
        QPushButton#stopButton {
            background-color: rgba(248,81,73,0.10);
            color: #f85149;
            border: 1px solid rgba(248,81,73,0.35);
            border-radius: 6px;
            padding: 6px 16px;
            min-height: 30px;
            min-width: 60px;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#stopButton:hover {
            background-color: rgba(248,81,73,0.20);
        }
        QPushButton#stopButton:disabled {
            background-color: #171b22;
            color: #5f6875;
            border: 1px solid #232b38;
        }
        QToolButton#itemButton {
            background-color: #10141a;
            border: 1px solid #1e2733;
            border-radius: 6px;
            color: #e6e9ee;
            font-size: 9px;
            font-weight: 600;
            padding: 4px;
            text-align: center;
        }
        QToolButton#itemButton:hover {
            background-color: #151a22;
            border-color: rgba(61,139,255,0.4);
        }

        QPushButton#successButton {
            background-color: rgba(63,185,80,0.10);
            color: #3fb950;
            border: 1px solid rgba(63,185,80,0.35);
            border-radius: 6px;
            padding: 6px 16px;
            min-height: 30px;
            min-width: 60px;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#successButton:hover {
            background-color: rgba(63,185,80,0.20);
        }
        QPushButton#dangerButton {
            background-color: rgba(248,81,73,0.10);
            color: #f85149;
            border: 1px solid rgba(248,81,73,0.35);
            border-radius: 6px;
            padding: 6px 16px;
            min-height: 30px;
            min-width: 60px;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#dangerButton:hover {
            background-color: rgba(248,81,73,0.20);
        }
        QPushButton#amberButton {
            background-color: rgba(210,153,34,0.10);
            color: #d29922;
            border: 1px solid rgba(210,153,34,0.35);
            border-radius: 6px;
            padding: 6px 16px;
            min-height: 30px;
            min-width: 60px;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#amberButton:hover {
            background-color: rgba(210,153,34,0.20);
        }
        QPushButton#ghostButton {
            background-color: transparent;
            color: #8b93a3;
            border: 1px solid #232b38;
            border-radius: 6px;
            padding: 5px 14px;
            min-height: 28px;
            min-width: 50px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton#ghostButton:hover {
            background-color: #141920;
            color: #ffffff;
            border-color: #3a4657;
        }

        /* ── Content ── */
        QStackedWidget#contentStack {
            background-color: #000000;
        }
        QWidget#pageContent {
            background-color: #000000;
        }
        QLabel#sectionHeader {
            font-size: 10px;
            font-weight: 800;
            color: #5f6875;
            letter-spacing: 1.4px;
            background-color: transparent;
            border: none;
            padding: 0px;
        }
        QFrame#sectionLine {
            background-color: #1b212b;
            max-height: 1px;
            border: none;
        }

        QWidget#cheatCard {
            background-color: #0e1116;
            border: 1px solid #1b212b;
            border-radius: 8px;
        }
        QWidget#cheatCard:hover {
            border-color: rgba(61,139,255,0.35);
            background-color: #10141a;
        }
        QLabel#toggleName {
            color: #e6e9ee;
            font-size: 13px;
            font-weight: 600;
            padding: 0px;
        }
        QLabel#toggleSubtitle {
            color: #677080;
            font-size: 11px;
            font-weight: 500;
            padding: 0px;
        }

        /* ── Inputs ── */
        QLineEdit#valueInput {
            background-color: #0a0d11;
            color: #e6e9ee;
            border: 1px solid #232b38;
            border-radius: 6px;
            padding: 5px 12px;
            font-size: 12px;
            font-family: "Cascadia Code", "Consolas", monospace;
            min-height: 26px;
            max-height: 26px;
            selection-background-color: #3d8bff;
            selection-color: #ffffff;
        }
        QLineEdit#valueInput:focus {
            border-color: #3d8bff;
        }
        QLineEdit#valueInput:hover {
            border-color: rgba(61,139,255,0.45);
        }

        QComboBox#toolCombo {
            background-color: #0a0d11;
            color: #e6e9ee;
            border: 1px solid #232b38;
            border-radius: 6px;
            padding: 5px 12px;
            font-size: 12px;
            min-height: 26px;
            max-height: 26px;
            min-width: 140px;
        }
        QComboBox#toolCombo:hover {
            border-color: rgba(61,139,255,0.45);
        }
        QComboBox#toolCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border: none;
            background-color: transparent;
        }
        QComboBox#toolCombo::down-arrow {
            image: none;
            width: 0px;
        }
        QComboBox#toolCombo QAbstractItemView {
            background-color: #0e1116;
            color: #e6e9ee;
            selection-background-color: rgba(61,139,255,0.22);
            selection-color: #ffffff;
            border: 1px solid #232b38;
            border-radius: 6px;
            outline: none;
            padding: 4px;
            font-size: 12px;
        }

        /* ── Lua editor & console ── */
        QPlainTextEdit#luaEditor {
            background-color: #0a0d11;
            color: #d4d9e2;
            border: 1px solid #1b212b;
            border-radius: 6px;
            padding: 10px;
            font-family: "Cascadia Code", "Consolas", "Courier New", monospace;
            font-size: 13px;
            selection-background-color: rgba(61,139,255,0.35);
            selection-color: #ffffff;
        }
        QPlainTextEdit#luaEditor:focus {
            border-color: rgba(61,139,255,0.55);
        }
        QWidget#luaLineArea {
            background-color: #0a0d11;
            border-top-left-radius: 6px;
            border-bottom-left-radius: 6px;
        }
        QTextEdit#luaConsole {
            background-color: #05070a;
            color: #9aa3b2;
            border: 1px solid #1b212b;
            border-radius: 6px;
            padding: 10px;
            font-family: "Cascadia Code", "Consolas", "Courier New", monospace;
            font-size: 12px;
            selection-background-color: rgba(61,139,255,0.35);
            selection-color: #ffffff;
        }
        QLabel#consoleLabel {
            font-size: 10px;
            font-weight: 800;
            color: #677080;
            letter-spacing: 1.2px;
            background-color: transparent;
            border: none;
        }
        QLabel#luaStatusLabel {
            color: #5aa3ff;
            font-size: 11px;
            font-weight: 600;
            background: transparent;
            border: none;
        }
        QLabel#luaStatusLabel[ok="false"] {
            color: #f85149;
        }
        QLabel#luaStatusLabel[ok="true"] {
            color: #3fb950;
        }
        QLabel#luaStatusLabel[idle="true"] {
            color: #8b93a3;
        }
        QWidget#luaToolbar {
            background: transparent;
            border-bottom: 1px solid #1b212b;
        }
        QWidget#luaStatusStrip {
            background: transparent;
            border-bottom: 1px solid #1b212b;
        }
        QFrame#toolSep {
            background-color: #1b212b;
            max-width: 1px;
            margin: 4px 2px;
            border: none;
        }
        QLabel#toolStatusCaption {
            color: #5f6875;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.8px;
            background: transparent;
            border: none;
        }
        QLabel#toolStatusValue {
            color: #8b93a3;
            font-size: 11px;
            font-weight: 600;
            background: transparent;
            border: none;
        }
        QLabel#toolStatusValue[ok="true"] {
            color: #3fb950;
        }
        QLabel#toolStatusValue[idle="true"] {
            color: #7b8494;
        }
        QLabel#vmChip {
            background-color: rgba(139,147,163,0.08);
            color: #8b93a3;
            border: 1px solid #2a3441;
            border-radius: 10px;
            padding: 2px 10px;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.4px;
        }
        QLabel#vmChip[on="true"] {
            background-color: rgba(63,185,80,0.10);
            color: #3fb950;
            border: 1px solid rgba(63,185,80,0.3);
        }
        QLabel#cursorPosLabel {
            color: #5f6875;
            font-size: 10px;
            font-weight: 600;
            background: transparent;
            border: none;
        }
        QPushButton#dangerGhostButton {
            background-color: transparent;
            color: #f85149;
            border: 1px solid rgba(248,81,73,0.35);
            border-radius: 6px;
            font-size: 11px;
            font-weight: 700;
            padding: 0 10px;
        }
        QPushButton#dangerGhostButton:hover {
            background-color: rgba(248,81,73,0.12);
            border-color: rgba(248,81,73,0.6);
        }
        QPushButton#dangerGhostButton:pressed {
            background-color: rgba(248,81,73,0.2);
        }
        QLabel#statusChip {
            background-color: rgba(61,139,255,0.12);
            color: #5aa3ff;
            border: 1px solid rgba(61,139,255,0.3);
            border-radius: 9px;
            padding: 0px 8px;
            font-size: 9px;
            font-weight: 700;
            letter-spacing: 0.4px;
        }
        QLabel#statusChip[pidOn="false"] {
            background-color: rgba(139,147,163,0.08);
            color: #8b93a3;
            border: 1px solid #2a3441;
        }
        QLabel#statusChip[pidOn="true"] {
            background-color: rgba(63,185,80,0.10);
            color: #3fb950;
            border: 1px solid rgba(63,185,80,0.3);
        }

        QScrollArea#sidebarScroll {
            background: transparent;
            border: none;
        }
        QScrollArea#sidebarScroll > QWidget > QWidget {
            background: transparent;
        }
        QWidget#sidebarTabList {
            background: transparent;
        }
        QLabel#tabLetter {
            border-radius: 6px;
            font-size: 13px;
            font-weight: 800;
            color: #9aa3b2;
            background-color: rgba(255,255,255,0.04);
            border: 1px solid #1b212b;
        }
        QLabel#tabLetter[hover="true"] {
            color: #ffffff;
            background-color: rgba(255,255,255,0.08);
            border-color: #2c3645;
        }
        QLabel#tabLetter[active="true"] {
            color: #ffffff;
            border: none;
            background-color: #3d8bff;
        }

        /* ── Script tabs (executor-style tab bar) ── */
        QTabWidget#scriptTabs::pane {
            border: 1px solid #1b212b;
            border-radius: 6px;
            background-color: #0a0d11;
            top: -1px;
        }
        QTabWidget#scriptTabs QTabBar::tab {
            background-color: transparent;
            color: #7b8494;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 7px 16px;
            margin-right: 2px;
            font-size: 12px;
            font-weight: 600;
        }
        QTabWidget#scriptTabs QTabBar::tab:hover {
            color: #c6ccd6;
        }
        QTabWidget#scriptTabs QTabBar::tab:selected {
            color: #5aa3ff;
            border-bottom: 2px solid #3d8bff;
        }
        QTabWidget#scriptTabs QTabBar::close-button {
            image: none;
        }

        /* ── Scrollbars ── */
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QScrollBar:vertical {
            background-color: transparent;
            width: 8px;
            border: none;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background-color: #262e3b;
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #3d8bff;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
            background-color: transparent;
        }
        QScrollBar:horizontal {
            background-color: transparent;
            height: 8px;
            border: none;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background-color: #262e3b;
            border-radius: 3px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #3d8bff;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
            background-color: transparent;
        }

        /* ── Splitter ── */
        QSplitter::handle {
            background-color: transparent;
        }
        QSplitter::handle:hover {
            background-color: rgba(61,139,255,0.25);
        }
        QSplitter::handle:horizontal {
            width: 3px;
            border-radius: 1px;
        }

        /* ── Tooltips & menus ── */
        QToolTip {
            background-color: #12161d;
            color: #e6e9ee;
            border: 1px solid #2b3441;
            border-radius: 5px;
            padding: 5px 9px;
            font-size: 11px;
        }
        QMenu {
            background-color: #0e1116;
            color: #e6e9ee;
            border: 1px solid #232b38;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 18px;
            border-radius: 5px;
            font-size: 12px;
        }
        QMenu::item:selected {
            background-color: rgba(61,139,255,0.18);
        }

        QWidget#togglePill {
            background-color: transparent;
        }
        QLabel#discordTitle {
            font-size: 15px;
            font-weight: 700;
            color: #ffffff;
            background: transparent;
            border: none;
        }
        QLabel#discordSub {
            font-size: 11px;
            color: #8b93a3;
            background: transparent;
            border: none;
        }
    )";
}

} // namespace ZelvexStyles