#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QColor>
#include <QString>
#include <QStringList>
#include <QSet>

// Full Lua lexer for the editor. Scans each block character-wise:
// block comments (--[[, --[=[), long strings ([[, [=[), quoted strings
// with escapes, line comments, hex/float/int numbers, keywords,
// game API globals (native.*, Game, World, Player, ...) and operators.
// Strings/comments always win over keywords since they are applied
// during the scan itself (no post-pass override needed).

class LuaHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit LuaHighlighter(QTextDocument* parent = nullptr)
        : QSyntaxHighlighter(parent) {
        m_kwFmt.setForeground(QColor("#C678DD"));
        m_kwFmt.setFontWeight(QFont::Bold);

        m_boolFmt.setForeground(QColor("#D19A66"));

        m_numFmt.setForeground(QColor("#D19A66"));

        m_strFmt.setForeground(QColor("#98C379"));

        m_commentFmt.setForeground(QColor("#5C6370"));

        m_callFmt.setForeground(QColor("#61AFEF"));

        m_opFmt.setForeground(QColor("#8A8AA0"));

        m_apiFmt.setForeground(QColor("#5AA3FF"));
        m_apiFmt.setFontWeight(QFont::Bold);

        m_keywords = {
            "and", "break", "do", "else", "elseif", "end", "for",
            "function", "goto", "if", "in", "local", "not", "or",
            "repeat", "return", "then", "until", "while"
        };
        m_bools = { "true", "false", "nil" };
        m_libs = {
            "string", "table", "math", "os", "io", "debug",
            "coroutine", "package", "bit32", "utf8"
        };
        m_api = {
            "Game", "World", "Player", "Actor", "Chat", "Backpack",
            "ScriptSupportEvent", "CustomUI", "RemoteFunction",
            "RemoteEvent", "MiniTimer", "ErrorCode", "CurEventParam",
            "print", "zout", "zelvex", "native", "Event", "Network",
            "Texture", "Sound", "Store", "Quest", "Skill", "Particle"
        };
        m_operators = {
            "...", "..", "==", "~=", "<=", ">=", "::", "=",
            "+", "-", "*", "/", "%", "^", "#", "<", ">",
            "(", ")", "[", "]", "{", "}", ",", ";", "."
        };
    }

protected:
    void highlightBlock(const QString& text) override {
        const int len = text.length();
        // block state: 0 normal, 1 block-comment(--[[), 2 long-string([[
        //              3 block-comment(--[=[), 4 long-string([=[)
        int state = previousBlockState();
        int i = 0;

        while (i < len) {
            const QChar c = text.at(i);

            if (state == 1 || state == 3) {
                const QString closer = (state == 3) ? "]=]" : "]]";
                int endIdx = text.indexOf(closer, i);
                if (endIdx < 0) {
                    setFormat(i, len - i, m_commentFmt);
                    setCurrentBlockState(state);
                    return;
                }
                int spanEnd = endIdx + closer.length();
                setFormat(i, spanEnd - i, m_commentFmt);
                i = spanEnd;
                state = 0;
                continue;
            }
            if (state == 2 || state == 4) {
                const QString closer = (state == 4) ? "]=]" : "]]";
                int endIdx = text.indexOf(closer, i);
                if (endIdx < 0) {
                    setFormat(i, len - i, m_strFmt);
                    setCurrentBlockState(state);
                    return;
                }
                int spanEnd = endIdx + closer.length();
                setFormat(i, spanEnd - i, m_strFmt);
                i = spanEnd;
                state = 0;
                continue;
            }

            // ── normal mode ──
            if (text.mid(i, 5) == "--[=[") {
                state = 3;
                i += 5;
                continue;
            }
            if (text.mid(i, 4) == "--[[") {
                state = 1;
                i += 4;
                continue;
            }
            if (text.mid(i, 2) == "--") {
                setFormat(i, len - i, m_commentFmt);
                break;
            }
            if (text.mid(i, 3) == "[=[") {
                state = 4;
                i += 3;
                continue;
            }
            if (text.mid(i, 2) == "[[") {
                state = 2;
                i += 2;
                continue;
            }
            if (c == '"' || c == '\'') {
                const QChar quote = c;
                int j = i + 1;
                while (j < len) {
                    if (text.at(j) == '\\') { j += 2; continue; }
                    if (text.at(j) == quote) { j++; break; }
                    j++;
                }
                if (j > len) j = len;
                setFormat(i, j - i, m_strFmt);
                i = j;
                continue;
            }
            if (c == '0' && i + 1 < len &&
                (text.at(i + 1) == 'x' || text.at(i + 1) == 'X')) {
                int j = i + 2;
                while (j < len && isHex(text.at(j))) j++;
                setFormat(i, j - i, m_numFmt);
                i = j;
                continue;
            }
            if (c.isDigit() || (c == '.' && i + 1 < len && text.at(i + 1).isDigit())) {
                int j = i;
                bool hasDot = false, hasExp = false;
                while (j < len) {
                    QChar d = text.at(j);
                    if (d.isDigit()) { j++; continue; }
                    if (d == '.' && !hasDot && !hasExp && c.isDigit()) {
                        hasDot = true; j++; continue;
                    }
                    if ((d == 'e' || d == 'E') && !hasExp && j + 1 < len) {
                        hasExp = true;
                        int k = j + 1;
                        if (text.at(k) == '+' || text.at(k) == '-') k++;
                        if (k < len && text.at(k).isDigit()) { j = k + 1; continue; }
                        hasExp = false;
                    }
                    break;
                }
                setFormat(i, j - i, m_numFmt);
                i = j;
                continue;
            }
            if (c.isLetter() || c == '_') {
                int j = i;
                while (j < len && (text.at(j).isLetterOrNumber() || text.at(j) == '_')) j++;
                const QString w = text.mid(i, j - i);
                const QChar nx = (j < len) ? text.at(j) : QChar();
                if (nx == '(' || nx == ':') {
                    setFormat(i, j - i, m_callFmt);
                } else if (m_keywords.contains(w)) {
                    setFormat(i, j - i, m_kwFmt);
                } else if (m_bools.contains(w)) {
                    setFormat(i, j - i, m_boolFmt);
                } else if (m_api.contains(w)) {
                    setFormat(i, j - i, m_apiFmt);
                } else if (m_libs.contains(w)) {
                    setFormat(i, j - i, m_callFmt);
                }
                i = j;
                continue;
            }
            bool opMatched = false;
            for (const QString& op : m_operators) {
                if (text.mid(i, op.length()) == op) {
                    setFormat(i, op.length(), m_opFmt);
                    i += op.length();
                    opMatched = true;
                    break;
                }
            }
            if (!opMatched) i++;
        }

        if (state != 0)
            setCurrentBlockState(state);
    }

private:
    static bool isHex(QChar c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    QTextCharFormat m_kwFmt, m_boolFmt, m_numFmt, m_strFmt,
                    m_commentFmt, m_callFmt, m_opFmt, m_apiFmt;
    QSet<QString> m_keywords, m_bools, m_libs, m_api;
    QStringList m_operators;
};