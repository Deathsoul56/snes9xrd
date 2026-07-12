#pragma once

#include <QHash>
#include <QLabel>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QWidget>

class QTimer;

class SnesControllerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SnesControllerWidget(QWidget *parent = nullptr);

    void setPressedNames(const QSet<QString> &snes_names);
    void setDebugRawState(const QString &raw);
    void clearPressed();

    QSize sizeHint() const override { return {560, 250}; }

signals:
    void buttonClicked(const QString &snes_name);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Hotspot
    {
        QString name;
        // Position is a ratio (0..1) over the source image rect.
        QPointF pos_ratio;
        qreal   radius_ratio;
    };

    void rebuildHotspots();
    QRectF imageRect() const;

    QPixmap image_;
    QList<Hotspot> hotspots_;
    QSet<QString> pressed_;
    QString debug_raw_;
};