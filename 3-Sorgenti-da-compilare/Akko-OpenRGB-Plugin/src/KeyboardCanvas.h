/*---------------------------------------------------------*\
| KeyboardCanvas.h                                         |
|                                                           |
|   QWidget that draws the Akko 3108 V2 layout and emits   |
|   a signal with the OpenRGB LED index when a key is       |
|   clicked.                                               |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#pragma once

#include <QWidget>
#include <QColor>
#include <vector>
#include "KeyboardLayout.h"     /* AKKO_KEYS, akko_key_layout[] */

class KeyboardCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit KeyboardCanvas(QWidget* parent = nullptr);

    void SetKeyColors(const std::vector<QColor>& colors);

signals:
    void KeyClicked(int led_index);

protected:
    void paintEvent(QPaintEvent* event)  override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event)   override;

private:
    std::vector<QColor> key_colors;
    std::vector<QRectF> key_rects;
    void RecomputeRects();
};