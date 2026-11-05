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
#include <QFontMetrics>

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

        /* Rettangolo tasto (rientrato di 1.5 px: spazio tra i tasti) */
        QRectF kr = r.adjusted(1.5, 1.5, -1.5, -1.5);
        p.setPen(QPen(QColor(70, 72, 80), 1.0));
        p.setBrush(c);
        p.drawRoundedRect(kr, 5.0, 5.0);

        /* Testo: dimensione proporzionale all'altezza del tasto */
        int fs = qBound(7, (int)(r.height() * 0.28), 18);
        f.setPixelSize(fs);
        p.setFont(f);

        /* Colore testo contrastante */
        float luma = 0.2126f * (float)c.redF() + 0.7152f * (float)c.greenF() + 0.0722f * (float)c.blueF();
        p.setPen(luma > 0.55f ? QColor(20, 20, 20) : QColor(235, 235, 235));

        QString label = QString::fromUtf8(k.label);

        /* Riduci il font finche' il testo sta nella larghezza del tasto */
        {
            int w_avail = (int)(kr.width() - 6);
            while(fs > 6)
            {
                f.setPixelSize(fs);
                QFontMetrics fm(f);
                if(fm.horizontalAdvance(label) <= w_avail)
                    break;
                fs--;
            }
            f.setPixelSize(fs);
            p.setFont(f);
        }

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
            p.drawText(kr, Qt::AlignVCenter | Qt::AlignHCenter, label);
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