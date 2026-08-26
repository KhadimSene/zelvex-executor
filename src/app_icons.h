#pragma once

// Vector icon set rendered with QPainter (no binary assets needed).
// All icons draw in a 24x24 logical space, rendered at 2x for crispness.
// Brand renders the actual Zelvex logo from resources.

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QLinearGradient>
#include <QFont>
#include <QFontDatabase>

namespace AppIcons {

enum class Id {
    Discord, Misc, Player, Teleport, Stats, Movement, Combat, Vision,
    Items, Lua, Sidebar, Min, Max, Close, Play, Trash, Clear,
    Refresh, Copy, Search, Check, Terminal, Brand, ChevronDown
};

// Segoe MDL2 Assets code points — Microsoft's native Windows 10+ icon font.
// These are DESIGNED UI icons (not Unicode text symbols), guaranteed crisp
// at any DPI.  Fallback: Segoe UI Symbol → hand-drawn QPainterPath.
inline const char* mdl2For(Id id) {
    switch (id) {
    case Id::Misc:      return u8"\uE713";  // Settings (gear)
    case Id::Player:    return u8"\uE77B";  // Contact (person)
    case Id::Teleport:  return u8"\uE707";  // MapPin (location)
    case Id::Stats:     return u8"\uE9D9";  // Chart (bar chart)
    case Id::Movement:  return u8"\uE965";  // Direction (arrow)
    case Id::Combat:    return u8"\uE727";  // Power (bolt)
    case Id::Vision:    return u8"\uE8B4";  // View (eye)
    case Id::Items:     return u8"\uE71C";  // AllApps (grid)
    case Id::Lua:       return u8"\uE711";  // Code (brackets)
    default:            return nullptr;
    }
}

// Legacy Unicode glyphs — kept as fallback if MDL2 unavailable.
inline const char* glyphFor(Id id) {
    switch (id) {
    case Id::Misc:      return u8"\u2699";
    case Id::Player:    return u8"\u263B";
    case Id::Teleport:  return u8"\u21C4";
    case Id::Stats:     return u8"\u2665";
    case Id::Movement:  return u8"\u26A1";
    case Id::Combat:    return u8"\u2694";
    case Id::Vision:    return u8"\u25C9";
    case Id::Items:     return u8"\u25A3";
    case Id::Lua:       return u8"\u2318";
    default:            return nullptr;
    }
}

inline QColor accentMain()   { return QColor("#3D8BFF"); }
inline QColor accentSoft()   { return QColor("#5AA3FF"); }
inline QColor textMuted()    { return QColor("#5F6875"); }
inline QColor textNormal()   { return QColor("#9AA3B2"); }
inline QColor textBright()   { return QColor("#E6E9EE"); }
inline QColor success()      { return QColor("#3FB950"); }
inline QColor danger()       { return QColor("#F85149"); }
inline QColor warn()         { return QColor("#D29922"); }
inline QColor info()         { return QColor("#38BDF8"); }

inline QPen strokePen(QColor c, qreal w = 2.0) {
    QPen pen(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    return pen;
}

inline void raw(QPainter& p, const QPainterPath& path) {
    p.drawPath(path);
}

inline QPixmap render(Id id, QColor color, int size = 24) {
    if (id == Id::Brand) {
        QPixmap logo(":/src/images/logo.png");
        if (!logo.isNull()) {
            QPixmap scaled = logo.scaled(size * 2, size * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(2.0);
            return scaled;
        }
        id = Id::Misc;
    }

    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Sidebar tab icons (Misc→Lua) always use QPainterPath vectors —
    // font-based glyphs (MDL2/Segoe) are unreliable across systems.
    // Render directly at output size with DPR=1 to avoid clipping issues.
    bool isTabIcon = (id == Id::Misc || id == Id::Player || id == Id::Teleport ||
                      id == Id::Stats || id == Id::Movement || id == Id::Combat ||
                      id == Id::Vision || id == Id::Items || id == Id::Lua);
    if (isTabIcon) {
        // Direct DPR=1 render — no intermediate 2x pixmap
        QPixmap direct(size, size);
        direct.fill(Qt::transparent);
        {
        QPainter dp(&direct);
        dp.setRenderHint(QPainter::Antialiasing);
        const qreal pad = 1.5;
        dp.translate(pad, pad);
        dp.scale((size - 3.0) / 24.0, (size - 3.0) / 24.0);
        dp.setPen(strokePen(color, 2.0));
        dp.setBrush(Qt::NoBrush);
        switch (id) {
        case Id::Misc: { // gear
            for (int i = 0; i < 8; i++) {
                dp.save();
                dp.translate(12, 12);
                dp.rotate(i * 45.0);
                dp.drawRoundedRect(QRectF(-1.4, -10.5, 2.8, 4.5), 1.2, 1.2);
                dp.restore();
            }
            dp.drawEllipse(QPointF(12, 12), 7.2, 7.2);
            dp.drawEllipse(QPointF(12, 12), 3.2, 3.2);
            break;
        }
        case Id::Player: { // person
            dp.drawEllipse(QPointF(12, 7.2), 3.6, 3.6);
            QPainterPath body;
            body.arcMoveTo(5.2, 13.5, 13.6, 9.5, 180);
            body.arcTo(5.2, 13.5, 13.6, 9.5, 180, 180);
            dp.drawPath(body);
            break;
        }
        case Id::Teleport: { // paper plane
            QPainterPath plane;
            plane.moveTo(20.2, 4.2);
            plane.lineTo(3.8, 11.0);
            plane.lineTo(9.6, 13.4);
            plane.lineTo(12.0, 19.4);
            plane.closeSubpath();
            dp.setBrush(QColor(color.red(), color.green(), color.blue(), 36));
            raw(dp, plane);
            dp.setBrush(Qt::NoBrush);
            QPainterPath fold;
            fold.moveTo(9.6, 13.4);
            fold.lineTo(20.2, 4.2);
            fold.lineTo(11.4, 17.8);
            dp.drawPath(fold);
            break;
        }
        case Id::Stats: { // heartbeat activity line
            QPainterPath pulse;
            pulse.moveTo(3.2, 13.2);
            pulse.lineTo(7.6, 13.2);
            pulse.lineTo(9.6, 9.4);
            pulse.lineTo(11.6, 14.8);
            pulse.lineTo(13.6, 10.2);
            pulse.lineTo(15.6, 13.2);
            pulse.lineTo(20.8, 13.2);
            dp.drawPath(pulse);
            dp.drawLine(QPointF(3.2, 3.5), QPointF(20.8, 3.5));
            dp.drawLine(QPointF(3.2, 3.5), QPointF(3.2, 20.5));
            dp.drawLine(QPointF(3.2, 20.5), QPointF(20.8, 20.5));
            break;
        }
        case Id::Movement: { // lightning bolt (filled)
            QPainterPath bolt;
            bolt.moveTo(13.6, 3.5);
            bolt.lineTo(7.2, 13.2);
            bolt.lineTo(11.0, 13.2);
            bolt.lineTo(10.4, 20.5);
            bolt.lineTo(16.8, 10.8);
            bolt.lineTo(13.0, 10.8);
            bolt.closeSubpath();
            dp.setBrush(color);
            dp.setPen(Qt::NoPen);
            raw(dp, bolt);
            dp.setPen(strokePen(color, 2.0));
            dp.setBrush(Qt::NoBrush);
            break;
        }
        case Id::Combat: { // sword
            QPainterPath blade;
            blade.moveTo(19.8, 4.6);
            blade.lineTo(10.4, 14.0);
            blade.lineTo(9.4, 9.8);
            blade.lineTo(4.6, 19.8);
            blade.lineTo(9.8, 19.6);
            dp.drawPath(blade);
            dp.drawLine(QPointF(10.4, 14.0), QPointF(9.4, 9.8));
            QPainterPath guard;
            guard.moveTo(3.6, 7.4);
            guard.lineTo(9.0, 6.4);
            dp.drawPath(guard);
            break;
        }
        case Id::Vision: { // eye
            QPainterPath eye;
            eye.moveTo(2.8, 12);
            eye.cubicTo(6.4, 5.2, 17.6, 5.2, 21.2, 12);
            eye.cubicTo(17.6, 18.8, 6.4, 18.8, 2.8, 12);
            eye.closeSubpath();
            dp.drawPath(eye);
            dp.setBrush(color);
            dp.setPen(Qt::NoPen);
            dp.drawEllipse(QPointF(12, 12), 3.0, 3.0);
            dp.setPen(strokePen(color, 2.0));
            dp.setBrush(Qt::NoBrush);
            break;
        }
        case Id::Items: { // package/box
            QPainterPath box;
            box.moveTo(4.0, 8.0);
            box.lineTo(4.0, 18.6);
            box.lineTo(20.0, 18.6);
            box.lineTo(20.0, 8.0);
            box.closeSubpath();
            dp.drawPath(box);
            dp.drawLine(QPointF(4.0, 8.0), QPointF(12.0, 12.2));
            dp.drawLine(QPointF(20.0, 8.0), QPointF(12.0, 12.2));
            dp.drawLine(QPointF(4.0, 8.0), QPointF(20.0, 8.0));
            break;
        }
        case Id::Lua: { // code brackets </>
            QPainterPath left;
            left.moveTo(10.0, 6.0);
            left.lineTo(5.0, 12.0);
            left.lineTo(10.0, 18.0);
            dp.drawPath(left);
            dp.drawLine(QPointF(12.0, 5.0), QPointF(12.0, 19.0));
            QPainterPath right;
            right.moveTo(14.0, 6.0);
            right.lineTo(19.0, 12.0);
            right.lineTo(14.0, 18.0);
            dp.drawPath(right);
            break;
        }
        default: break;
        }
        }
        return direct;
    }

    const char* glyph = mdl2For(id);
    const char* fallback = (!glyph) ? glyphFor(id) : nullptr;
    const char* use = glyph ? glyph : fallback;
    if (use) {
        QFont gf;
        QFontDatabase fdb;
        if (!glyph && fallback) {
            if (fdb.families().contains("Segoe UI Symbol"))
                gf.setFamily("Segoe UI Symbol");
            else
                gf.setFamily("Segoe UI");
        } else {
            if (fdb.families().contains("Segoe MDL2 Assets"))
                gf.setFamily("Segoe MDL2 Assets");
            else if (fdb.families().contains("Segoe Fluent Icons"))
                gf.setFamily("Segoe Fluent Icons");
            else if (fdb.families().contains("Segoe UI Symbol"))
                gf.setFamily("Segoe UI Symbol");
            else
                gf.setFamily("Segoe UI");
        }
        gf.setPixelSize(qRound(size * (glyph ? 0.85 : 0.75)));
        p.setFont(gf);
        p.setPen(color);
        p.drawText(QRectF(0, 0, (qreal)size, (qreal)size),
                   Qt::AlignCenter, QString::fromUtf8(use));
        return pm;
    }

    p.scale(2.0, 2.0);
    const qreal pad = 1.5;
    p.translate(pad, pad);
    p.scale((size - 3.0) / 24.0, (size - 3.0) / 24.0);
    p.setPen(strokePen(color, 2.0));
    p.setBrush(Qt::NoBrush);

    switch (id) {
    case Id::Discord: {
        // game controller silhouette
        QPainterPath body;
        body.moveTo(5, 8);
        body.cubicTo(5, 5.5, 7, 4.5, 10, 4.5);
        body.lineTo(14, 4.5);
        body.cubicTo(17, 4.5, 19, 5.5, 19, 8);
        body.lineTo(20.2, 15.2);
        body.cubicTo(20.6, 17.6, 18.4, 19.6, 16.2, 18.9);
        body.lineTo(13.6, 18.0);
        body.lineTo(10.4, 18.0);
        body.lineTo(7.8, 18.9);
        body.cubicTo(5.6, 19.6, 3.4, 17.6, 3.8, 15.2);
        body.closeSubpath();
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 26));
        raw(p, body);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(8.2, 11.5), 1.1, 1.1);
        p.drawEllipse(QPointF(15.8, 11.5), 1.1, 1.1);
        QPainterPath dpad;
        dpad.addRoundedRect(QRectF(10.8, 9.2, 2.4, 4.6), 1, 1);
        dpad.addRoundedRect(QRectF(9.7, 10.3, 4.6, 2.4), 1, 1);
        p.drawPath(dpad);
        break;
    }
    case Id::Misc: { // gear
        for (int i = 0; i < 8; i++) {
            p.save();
            p.translate(12, 12);
            p.rotate(i * 45.0);
            p.drawRoundedRect(QRectF(-1.4, -10.5, 2.8, 4.5), 1.2, 1.2);
            p.restore();
        }
        p.drawEllipse(QPointF(12, 12), 7.2, 7.2);
        p.drawEllipse(QPointF(12, 12), 3.2, 3.2);
        break;
    }
    case Id::Player: { // person
        p.drawEllipse(QPointF(12, 7.2), 3.6, 3.6);
        QPainterPath body;
        body.arcMoveTo(5.2, 13.5, 13.6, 9.5, 180);
        body.arcTo(5.2, 13.5, 13.6, 9.5, 180, 180);
        p.drawPath(body);
        break;
    }
    case Id::Teleport: { // paper plane
        QPainterPath plane;
        plane.moveTo(20.2, 4.2);
        plane.lineTo(3.8, 11.0);
        plane.lineTo(9.6, 13.4);
        plane.lineTo(12.0, 19.4);
        plane.closeSubpath();
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 36));
        raw(p, plane);
        p.setBrush(Qt::NoBrush);
        QPainterPath fold;
        fold.moveTo(9.6, 13.4);
        fold.lineTo(20.2, 4.2);
        fold.lineTo(11.4, 17.8);
        p.drawPath(fold);
        break;
    }
    case Id::Stats: { // heartbeat activity line
        QPainterPath pulse;
        pulse.moveTo(3.2, 13.2);
        pulse.lineTo(7.6, 13.2);
        pulse.lineTo(9.6, 9.4);
        pulse.lineTo(11.6, 14.8);
        pulse.lineTo(13.6, 10.2);
        pulse.lineTo(15.6, 13.2);
        pulse.lineTo(20.8, 13.2);
        p.drawPath(pulse);
        p.drawLine(QPointF(3.2, 3.5), QPointF(20.8, 3.5));
        p.drawLine(QPointF(3.2, 3.5), QPointF(3.2, 20.5));
        p.drawLine(QPointF(3.2, 20.5), QPointF(20.8, 20.5));
        break;
    }
    case Id::Movement: { // lightning bolt (filled)
        QPainterPath bolt;
        bolt.moveTo(13.6, 3.5);
        bolt.lineTo(7.2, 13.2);
        bolt.lineTo(11.0, 13.2);
        bolt.lineTo(10.4, 20.5);
        bolt.lineTo(16.8, 10.8);
        bolt.lineTo(13.0, 10.8);
        bolt.closeSubpath();
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        raw(p, bolt);
        break;
    }
    case Id::Combat: { // sword
        QPainterPath blade;
        blade.moveTo(19.8, 4.6);
        blade.lineTo(10.4, 14.0);
        blade.lineTo(9.4, 9.8);
        blade.lineTo(4.6, 19.8);
        blade.lineTo(9.8, 19.6);
        p.drawPath(blade);
        p.drawLine(QPointF(10.4, 14.0), QPointF(9.4, 9.8));
        QPainterPath guard;
        guard.moveTo(3.6, 7.4);
        guard.lineTo(9.0, 6.4);
        p.drawPath(guard);
        break;
    }
    case Id::Vision: { // eye
        QPainterPath eye;
        eye.moveTo(2.8, 12);
        eye.cubicTo(6.4, 5.2, 17.6, 5.2, 21.2, 12);
        eye.cubicTo(17.6, 18.8, 6.4, 18.8, 2.8, 12);
        eye.closeSubpath();
        p.drawPath(eye);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(12, 12), 3.0, 3.0);
        break;
    }
    case Id::Items: { // package/box
        QPainterPath box;
        box.moveTo(4.0, 8.0);
        box.lineTo(4.0, 18.6);
        box.lineTo(20.0, 18.6);
        box.lineTo(20.0, 8.0);
        box.closeSubpath();
        p.drawPath(box);
        p.drawLine(QPointF(4.0, 8.0), QPointF(12.0, 12.2));
        p.drawLine(QPointF(20.0, 8.0), QPointF(12.0, 12.2));
        p.drawLine(QPointF(4.0, 8.0), QPointF(20.0, 8.0));
        break;
    }
    case Id::Lua: { // code brackets </>
        // Left angle bracket <
        QPainterPath left;
        left.moveTo(10.0, 6.0);
        left.lineTo(5.0, 12.0);
        left.lineTo(10.0, 18.0);
        p.drawPath(left);
        // Slash /
        p.drawLine(QPointF(12.0, 5.0), QPointF(12.0, 19.0));
        // Right angle bracket >
        QPainterPath right;
        right.moveTo(14.0, 6.0);
        right.lineTo(19.0, 12.0);
        right.lineTo(14.0, 18.0);
        p.drawPath(right);
        break;
    }
    case Id::Sidebar: { // panel toggle
        p.drawRoundedRect(QRectF(3.4, 4.4, 17.2, 15.2), 2.5, 2.5);
        QPainterPath inner;
        inner.moveTo(9.4, 4.4);
        inner.lineTo(9.4, 19.6);
        p.drawPath(inner);
        break;
    }
    case Id::Min: {
        p.drawLine(QPointF(6.0, 12.0), QPointF(18.0, 12.0));
        break;
    }
    case Id::Max: {
        p.drawRoundedRect(QRectF(6.0, 5.5, 12.0, 13.0), 2.0, 2.0);
        break;
    }
    case Id::Close: {
        p.drawLine(QPointF(6.4, 6.4), QPointF(17.6, 17.6));
        p.drawLine(QPointF(17.6, 6.4), QPointF(6.4, 17.6));
        break;
    }
    case Id::Play: {
        QPainterPath tri;
        tri.moveTo(8.4, 5.6);
        tri.lineTo(18.8, 12.0);
        tri.lineTo(8.4, 18.4);
        tri.closeSubpath();
        p.fillPath(tri, color);
        break;
    }
    case Id::Trash: {
        p.drawRoundedRect(QRectF(6.0, 8.4, 12.0, 12.8), 2.0, 2.0);
        p.drawLine(QPointF(4.4, 8.4), QPointF(19.6, 8.4));
        p.drawLine(QPointF(9.4, 4.4), QPointF(14.6, 4.4));
        p.drawLine(QPointF(9.4, 4.4), QPointF(9.4, 8.4));
        p.drawLine(QPointF(14.6, 4.4), QPointF(14.6, 8.4));
        break;
    }
    case Id::Clear: { // x in circle
        p.drawEllipse(QPointF(12, 12), 8.4, 8.4);
        p.drawLine(QPointF(9.0, 9.0), QPointF(15.0, 15.0));
        p.drawLine(QPointF(15.0, 9.0), QPointF(9.0, 15.0));
        break;
    }
    case Id::Refresh: {
        QPainterPath arc;
        arc.moveTo(17.6, 8.4);
        arc.arcTo(QRectF(5.4, 5.4, 13.2, 13.2), -45, 195);
        p.drawPath(arc);
        QPainterPath head;
        head.moveTo(17.6, 4.4);
        head.lineTo(20.6, 8.4);
        head.lineTo(15.6, 9.6);
        head.closeSubpath();
        p.drawPath(head);
        break;
    }
    case Id::Copy: {
        p.drawRoundedRect(QRectF(8.4, 4.4, 11.2, 11.2), 2.0, 2.0);
        QPainterPath back;
        back.addRoundedRect(QRectF(4.4, 8.4, 11.2, 11.2), 2.0, 2.0);
        p.drawPath(back);
        break;
    }
    case Id::Search: {
        p.drawEllipse(QPointF(10.4, 10.4), 5.6, 5.6);
        p.drawLine(QPointF(14.8, 14.8), QPointF(19.4, 19.4));
        break;
    }
    case Id::Check: {
        p.drawEllipse(QPointF(12, 12), 8.4, 8.4);
        p.drawLine(QPointF(8.2, 12.4), QPointF(11.0, 15.0));
        p.drawLine(QPointF(11.0, 15.0), QPointF(16.0, 9.2));
        break;
    }
    case Id::ChevronDown: {
        p.drawLine(QPointF(6.0, 9.6), QPointF(12.0, 15.4));
        p.drawLine(QPointF(18.0, 9.6), QPointF(12.0, 15.4));
        break;
    }
    case Id::Terminal: {
        QPainterPath frame;
        frame.addRoundedRect(QRectF(3.4, 5.0, 17.2, 14.0), 3, 3);
        p.drawPath(frame);
        p.drawLine(QPointF(7.0, 10.2), QPointF(17.0, 10.2));
        p.drawLine(QPointF(7.0, 14.2), QPointF(17.0, 14.2));
        break;
    }
    case Id::Brand: {
        break; // handled above (logo.png)
    }
    }
    return pm;
}

inline QIcon icon(Id id, QColor color, int size = 24) {
    return QIcon(render(id, color, size));
}

inline QPixmap pixmap(Id id, QColor color, int size = 24) {
    return render(id, color, size);
}

// DPI-independent variant: returns a pixmap whose LOGICAL size is exactly
// `size` with devicePixelRatio 1.0, so QLabel never clips it regardless of
// the screen scale factor.
inline QPixmap labelPixmap(Id id, QColor color, int size = 24) {
    QPixmap src = render(id, color, size);
    // Tab icons are already rendered at DPR=1 in exact size — pass through
    if (src.devicePixelRatio() <= 1.0 && src.width() == size && src.height() == size)
        return src;
    QPixmap dst(size, size);
    dst.fill(Qt::transparent);
    {
        QPainter p(&dst);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(QRect(0, 0, size, size), src);
    }
    return dst;
}

} // namespace AppIcons