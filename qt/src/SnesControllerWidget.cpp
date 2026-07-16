#include "SnesControllerWidget.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <algorithm>

SnesControllerWidget::SnesControllerWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(520, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_StyledBackground, false);

    image_ = QPixmap(QStringLiteral(":/controllers/snes_controller.png"));

    rebuildHotspots();
}

void SnesControllerWidget::rebuildHotspots()
{
    // Hotspot centres and click radii are expressed as ratios of the source
    // image's actual size (529x238, snes_controller.png). These were measured
    // directly from the button graphics (pixel bounding boxes of the D-pad
    // cross and each coloured face button, and the Select/Start pill
    // outlines), not guessed, so the highlight ellipse lines up with the
    // printed button underneath it.
    hotspots_.clear();
    hotspots_ = {
        // D-pad arms (cross centred at ~114.5,117.5; half-extent ~42.5px).
        { QStringLiteral("Up"),    { 0.217f, 0.395f }, 0.060f },
        { QStringLiteral("Right"), { 0.261f, 0.494f }, 0.060f },
        { QStringLiteral("Down"),  { 0.217f, 0.592f }, 0.060f },
        { QStringLiteral("Left"),  { 0.172f, 0.494f }, 0.060f },

        // System pills.
        { QStringLiteral("Select"), { 0.413f, 0.565f }, 0.075f },
        { QStringLiteral("Start"),  { 0.515f, 0.565f }, 0.075f },

        // Face buttons (X top, Y left, A right, B bottom in diamond).
        { QStringLiteral("X"), { 0.772f, 0.331f }, 0.078f },
        { QStringLiteral("Y"), { 0.680f, 0.487f }, 0.078f },
        { QStringLiteral("A"), { 0.867f, 0.503f }, 0.078f },
        { QStringLiteral("B"), { 0.775f, 0.658f }, 0.078f },

        // Shoulders (drawn as labels above the image).
        { QStringLiteral("L"), { 0.090f, 0.100f }, 0.055f },
        { QStringLiteral("R"), { 0.910f, 0.100f }, 0.055f },
    };
}

QRectF SnesControllerWidget::imageRect() const
{
    if (image_.isNull()) return rect();

    // Scale image to fit width while preserving aspect ratio, then center it
    // vertically. Reserve a strip above for the shoulder buttons.
    const qreal aspect = qreal(image_.width()) / qreal(image_.height());
    qreal w = width();
    qreal h = w / aspect;
    if (h > height() - 30) // 30px for shoulders
    {
        h = height() - 30;
        w = h * aspect;
    }
    qreal x = (width() - w) / 2.0;
    qreal y = 30 + (height() - 30 - h) / 2.0;
    return QRectF(x, y, w, h);
}

void SnesControllerWidget::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().color(QPalette::Window)); // always match the active theme

    if (image_.isNull())
    {
        p.setPen(QColor("#8a8d99"));
        p.drawText(rect(), Qt::AlignCenter, "snes_controller.png not loaded");
        return;
    }

    QRectF img = imageRect();
    p.drawPixmap(img.toRect(), image_);

    // Debug overlays.
    p.setPen(QColor("#7a808c"));
    QFont f = p.font();
    f.setPointSizeF(9);
    p.setFont(f);
    if (!pressed_.isEmpty())
    {
        QStringList sorted = pressed_.values();
        std::sort(sorted.begin(), sorted.end());
        p.drawText(QRectF(0, height() - 32, width(), 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   "highlighted SNES: " + sorted.join(" "));
    }
    if (!debug_raw_.isEmpty())
    {
        p.drawText(QRectF(0, height() - 16, width(), 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   "raw SDL: " + debug_raw_);
    }

    // Per-button hardware colour (used to fill the highlight when pressed).
    auto base_color = [](const QString &name) -> QColor {
        if (name == "A") return QColor("#d54a4a"); // red
        if (name == "B") return QColor("#dfc544"); // yellow
        if (name == "X") return QColor("#3e6fc9"); // blue
        if (name == "Y") return QColor("#4aa564"); // green
        if (name == "L" || name == "R") return QColor("#9097a3");
        if (name == "Start" || name == "Select") return QColor("#5e6168");
        return QColor("#f0f0f0"); // d-pad arrows
    };

    for (const auto &h : hotspots_)
    {
        QPointF c(img.x() + img.width() * h.pos_ratio.x(),
                  img.y() + img.height() * h.pos_ratio.y());
        // Same ratio used for hit-testing in mousePressEvent, so the glow
        // always lines up exactly with what registers as a click.
        qreal r = img.height() * h.radius_ratio;

        if (pressed_.contains(h.name))
        {
            QColor fill = base_color(h.name).lighter(140);
            p.setPen(QPen(QColor(255, 255, 255, 240), 2.5));
            p.setBrush(QBrush(fill));
            p.drawEllipse(c, r, r);

            // Outer halo for extra pop
            QRadialGradient halo(c, r * 1.8);
            QColor glow = base_color(h.name);
            halo.setColorAt(0.0, QColor(glow.red(), glow.green(), glow.blue(), 150));
            halo.setColorAt(1.0, QColor(glow.red(), glow.green(), glow.blue(), 0));
            p.setPen(Qt::NoPen);
            p.setBrush(QBrush(halo));
            p.drawEllipse(c, r * 1.8, r * 1.8);
        }

        // Shoulder labels above the image.
        if (h.name == "L" || h.name == "R")
        {
            QFont f = p.font();
            f.setBold(true);
            f.setPointSizeF(12);
            p.setFont(f);
            p.setPen(pressed_.contains(h.name) ? QColor("#ffffff") : QColor("#a8aab2"));
            QRectF label_rect(c.x() - 24, 0, 48, 22);
            p.drawText(label_rect, Qt::AlignCenter, h.name);
        }
    }
}

void SnesControllerWidget::mousePressEvent(QMouseEvent *event)
{
    if (image_.isNull()) return;

    QRectF img = imageRect();
    for (const auto &h : hotspots_)
    {
        QPointF c(img.x() + img.width() * h.pos_ratio.x(),
                  img.y() + img.height() * h.pos_ratio.y());
        qreal r = img.height() * h.radius_ratio;
        QPointF delta = event->pos() - c;
        if (delta.x() * delta.x() + delta.y() * delta.y() <= r * r)
        {
            emit buttonClicked(h.name);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void SnesControllerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void SnesControllerWidget::setPressedNames(const QSet<QString> &snes_names)
{
    if (pressed_ == snes_names) return;
    pressed_ = snes_names;
    update();
}

void SnesControllerWidget::setDebugRawState(const QString &raw)
{
    if (debug_raw_ == raw) return;
    debug_raw_ = raw;
    update();
}

void SnesControllerWidget::clearPressed()
{
    if (pressed_.isEmpty()) return;
    pressed_.clear();
    update();
}