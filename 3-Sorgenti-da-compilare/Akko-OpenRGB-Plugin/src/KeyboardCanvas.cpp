/*---------------------------------------------------------*\
| KeyboardCanvas.cpp                                       |
|                                                           |
|   Draws the Akko 3108 V2 keyboard layout.                |
|   Each key is a rounded rectangle filled with its LED     |
|   color; labels (including multilabel keys like NumEnter) |
|   are centred inside.                                     |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#include "KeyboardCanvas.h"
#include <QPainter>
#include <QMouseEvent>

KeyboardCanvas::KeyboardCanvas(QWidget* parent) :
    QWidget(parent)
{
    key_colors.assign(AKKO_KEYS, QColor(0, 0, 0));
    setMouseTracking(false);
    setMinimumHeight(180);
}

void KeyboardCanvas::SetKeyColors(const std::vector<QColor>& colors)
{
    key_colors = colors;
    update();
}

void KeyboardCanvas::RecomputeRects()
{
    key_rects.clear();
    key_rects.reserve(AKKO_KEYS);

    /* Margine attorno all'area tasti, in pixel */
    const float MARGIN = 12.0f;
    float avail_w = (float)(width()  - 2 * MARGIN);
    float avail_h = (float)(height() - 2 * MARGIN);

    if(avail_w < 1 || avail_h < 1)
        return;

    for(int i = 0; i < AKKO_KEYS; i++)
    {
        const AkkoKeyShape& k = akko_key_layout[i];
        float x = MARGIN + k.nx * avail_w;
        float y = MARGIN + k.ny * avail_h;
        float w = k.nw * avail_w;
        float h = k.nh * avail_h;
        key_rects.push_back(QRectF(x, y, w, h));
    }
}

void KeyboardCanvas::resizeEvent(QResizeEvent* /*event*/)
{
    RecomputeRects();
}

void KeyboardCanvas::paintEvent(QPaintEvent* /*event*/)
{
    if((int)key_rects.size() != AKKO_KEYS)
        RecomputeRects();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    /* Sfondo scuro coerente col tema OpenRGB */
    p.fillRect(rect(), QColor(30, 32, 38));

    QFont f = p.font();
    int label_count = AKKO_KEYS < (int)key_rects.size() ? AKKO_KEYS : (int)key_rects.size();

    for(int i = 0; i < label_count; i++)
    {
        const AkkoKeyShape& k = akko_key_layout[i];
        const QRectF& r       = key_rects[i];
        QColor c              = (i < (int)key_colors.size()) ? key_colors[i] : QColor(0, 0, 0);

        /* Rettangolo tasto */
        p.setPen(QPen(QColor(60, 60, 66), 1.0));
        p.setBrush(c);
        p.drawRoundedRect(r, 4.0, 4.0);

        /* Testo: dimensione proporzionale all'altezza del tasto */
        int fs = qBound(7, (int)(r.height() * 0.28), 18);
        f.setPixelSize(fs);
        p.setFont(f);

        /* Colore testo contrastante */
        unsigned int luma = (unsigned int)(0.2126f * c.redF() + 0.7152f * c.greenF() + 0.0722f * c.blueF());
        p.setPen(luma > 0.5 ? QColor(20, 20, 20) : QColor(230, 230, 230));

        QString label = QString::fromUtf8(k.label);

        if(label.contains('\n'))
        {
            QStringList parts = label.split('\n');
            QRectF upper = r;
            upper.adjust(0, 0, 0, -r.height() * 0.02);
            upper.setHeight(r.height() * 0.52);
            p.drawText(upper, Qt::AlignVCenter | Qt::AlignHCenter, parts[0]);

            QRectF lower = r;
            lower.adjust(0, r.height() * 0.48, 0, 0);
            p.drawText(lower, Qt::AlignVCenter | Qt::AlignHCenter, parts[1]);
        }
        else
        {
            p.drawText(r, Qt::AlignVCenter | Qt::AlignHCenter, label);
        }
    }
}

void KeyboardCanvas::mousePressEvent(QMouseEvent* event)
{
    if((int)key_rects.size() != AKKO_KEYS)
        return;

    QPointF pos = event->localPos();
    for(int i = 0; i < AKKO_KEYS; i++)
    {
        if(key_rects[i].contains(pos))
        {
            emit KeyClicked(i);
            return;
        }
    }
}