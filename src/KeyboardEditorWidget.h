/*---------------------------------------------------------*\
| KeyboardEditorWidget.h                                    |
|                                                           |
|   Container widget for the Akko 3108 V2 plugin page.      |
|   Combines the keyboard canvas with a color palette and    |
|   controls.                                               |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#pragma once

#include <QWidget>
#include <QColor>
#include <vector>
#include <QLabel>
#include <QPushButton>
#include "RGBControllerInterface.h"
#include "OpenRGBPluginInterface.h"
#include "KeyboardLayout.h"     /* AKKO_KEYS */

class KeyboardCanvas;
class QTimer;

class KeyboardEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit KeyboardEditorWidget(OpenRGBPluginAPIInterface* api,
                                  QWidget* parent = nullptr);

private slots:
    void OnTimerTick();
    void OnKeyClicked(int led_index);
    void OnPickColor();
    void OnClearAll();
    void OnToggleDirectMode(bool checked);
    void OnLoadFromKeyboard();

private:
    void FindController();
    int  FindCustomModeIndex();
    void EnterCustomMode();
    void SendBuffer();
    void RefreshColors();
    void SetCurrentColor(const QColor& c);
    void UpdateStatus(const QString& msg, bool ok = true);

    OpenRGBPluginAPIInterface*  api;
    RGBControllerInterface*     controller;
    int                         custom_mode;   /* zone 0 index of "Custom" mode */
    bool                        found_flag;
    std::vector<QColor>         key_colors;
    QColor                      current_color;

    KeyboardCanvas*             canvas;
    QLabel*                     status_label;
    QLabel*                     current_color_label;
    QTimer*                     timer;
    QPushButton*                direct_mode_btn;
    std::vector<QPushButton*>   swatch_buttons;
};