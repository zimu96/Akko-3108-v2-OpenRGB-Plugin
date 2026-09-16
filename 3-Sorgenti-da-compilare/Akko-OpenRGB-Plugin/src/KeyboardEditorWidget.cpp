/*---------------------------------------------------------*\
| KeyboardEditorWidget.cpp                                  |
|                                                           |
|   Container widget for the Akko 3108 V2 plugin page.      |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#include "KeyboardEditorWidget.h"
#include "KeyboardCanvas.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QTimer>
#include <QPainter>

/*---------------------------------------------------------*\
| RGBColor helpers (same format as RGBControllerInterface:   |
|   byte 0=R, byte 1=G, byte 2=B, byte 3=0)                |
\*---------------------------------------------------------*/
static inline RGBColor QtToRGBColor(const QColor& c)
{
    return((RGBColor)c.red()
         | ((RGBColor)c.green() << 8)
         | ((RGBColor)c.blue()  << 16));
}

static inline QColor RGBColorToQt(RGBColor rgb)
{
    return QColor::fromRgb((rgb >>  0) & 0xFF,
                           (rgb >>  8) & 0xFF,
                           (rgb >> 16) & 0xFF);
}

/*---------------------------------------------------------*\
| Constructor                                                |
\*---------------------------------------------------------*/
KeyboardEditorWidget::KeyboardEditorWidget(OpenRGBPluginAPIInterface* api_ptr,
                                           QWidget* parent)
    : QWidget(parent)
    , api(api_ptr)
    , controller(nullptr)
    , custom_mode(-1)
    , found_flag(false)
    , current_color(0xFF, 0x00, 0x00)   /* default red */
{
    key_colors.assign(AKKO_KEYS, QColor(0, 0, 0));

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(6, 6, 6, 6);
    main_layout->setSpacing(6);

    /*-----------------------------------------------------*\
    | Canvas                                                 |
    \*-----------------------------------------------------*/
    canvas = new KeyboardCanvas(this);
    main_layout->addWidget(canvas, 1);

    connect(canvas, &KeyboardCanvas::KeyClicked,
            this,   &KeyboardEditorWidget::OnKeyClicked);

    /*-----------------------------------------------------*\
    | Controls row                                           |
    \*-----------------------------------------------------*/
    QHBoxLayout* ctrl = new QHBoxLayout();
    ctrl->setSpacing(4);

    /* Colore corrente — quadretto 28x28 + bordo */
    current_color_label = new QLabel(this);
    current_color_label->setFixedSize(28, 28);
    current_color_label->setToolTip("Colore selezionato");
    current_color_label->setAutoFillBackground(true);
    ctrl->addWidget(current_color_label);

    /* Palette: 10 colori comuni */
    const QColor palette[] =
    {
        {0xFF, 0x00, 0x00},   /* Rosso     */
        {0x00, 0xFF, 0x00},   /* Verde     */
        {0x00, 0x00, 0xFF},   /* Blu       */
        {0x00, 0xFF, 0xFF},   /* Ciano     */
        {0xFF, 0x00, 0xFF},   /* Magenta   */
        {0xFF, 0xFF, 0x00},   /* Giallo    */
        {0xFF, 0xFF, 0xFF},   /* Bianco    */
        {0xFF, 0x8C, 0x00},   /* Arancione */
        {0x80, 0x00, 0xFF},   /* Viola     */
        {0xFF, 0x55, 0x00},   /* Arancio scuro */
    };

    for(const QColor& c : palette)
    {
        QPushButton* b = new QPushButton(this);
        b->setFixedSize(26, 26);
        b->setToolTip(c.name());
        b->setStyleSheet(
            QString("QPushButton{ background-color:%1; border:2px solid #555; border-radius:3px; }"
                    "QPushButton:pressed{ border:2px solid #fff; }")
                .arg(c.name()));
        connect(b, &QPushButton::clicked, this, [this, c](){ SetCurrentColor(c); });
        ctrl->addWidget(b);
        swatch_buttons.push_back(b);
    }
    SetCurrentColor(current_color);

    ctrl->addSpacing(10);

    /* Bottone "Colore..." → QColorDialog */
    QPushButton* pick_btn = new QPushButton("Colore…", this);
    connect(pick_btn, &QPushButton::clicked, this, &KeyboardEditorWidget::OnPickColor);
    ctrl->addWidget(pick_btn);

    /* Bottone "Tutti neri" → reset buffer a nero */
    QPushButton* clear_btn = new QPushButton("Tutti neri", this);
    connect(clear_btn, &QPushButton::clicked, this, &KeyboardEditorWidget::OnClearAll);
    ctrl->addWidget(clear_btn);

    /* Bottone "Modalità Direct" → alternativa per salvare */
    direct_mode_btn = new QPushButton("Direct OFF", this);
    direct_mode_btn->setCheckable(true);
    direct_mode_btn->setToolTip(
        "Abilita la modalità Direct per rispecchiare automaticamente "
        "i cambiamenti nel buffer per-key. Non necessaria per la "
        "Akko 3108 V2: il tasto viene aggiornato ad ogni clic.");
    connect(direct_mode_btn, &QPushButton::toggled,
            this,            &KeyboardEditorWidget::OnToggleDirectMode);
    ctrl->addWidget(direct_mode_btn);

    ctrl->addSpacing(10);

    /* Bottone "Carica da tastiera" (ricarica i colori dal buffer OpenRGB) */
    QPushButton* reload_btn = new QPushButton("Aggiorna", this);
    reload_btn->setToolTip("Rilegge i colori dal buffer OpenRGB e aggiorna la vista");
    connect(reload_btn, &QPushButton::clicked,
            this,       &KeyboardEditorWidget::OnLoadFromKeyboard);
    ctrl->addWidget(reload_btn);

    ctrl->addStretch(1);

    /* Status label */
    status_label = new QLabel("Ricerca tastiera Akko…", this);
    status_label->setStyleSheet("color:#aaa;");
    ctrl->addWidget(status_label);

    main_layout->addLayout(ctrl);

    /*-----------------------------------------------------*\
    | Timer: monitora la tastiera                           |
    \*-----------------------------------------------------*/
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &KeyboardEditorWidget::OnTimerTick);
    timer->start(700);
}

/*---------------------------------------------------------*\
| Timer tick                                                |
\*---------------------------------------------------------*/
void KeyboardEditorWidget::OnTimerTick()
{
    if(controller == nullptr)
    {
        FindController();

        if(controller == nullptr)
        {
            if(!found_flag)
                UpdateStatus("Tastiera Akko non trovata — riproverò…", false);
            return;
        }

        /* Trovata! */
        found_flag = true;
        custom_mode = FindCustomModeIndex();
        QString msg = QString("Akko 3108 V2 rilevata — LED: %1 — "
                              "seleziona un colore e clicca i tasti")
                          .arg(controller->GetLEDCount());
        UpdateStatus(msg, true);

        /* Porta la tastiera in modalità Custom (per-key) e
           invia il buffer, così i clic funzionano subito. */
        EnterCustomMode();

        RefreshColors();
        return;
    }

    /* Rileggi i colori ogni tick per riflettere esterni (GUI, profili) */
    RefreshColors();
}

/*---------------------------------------------------------*\
| Find the controller by name                               |
\*---------------------------------------------------------*/
void KeyboardEditorWidget::FindController()
{
    if(api == nullptr)
        return;

    std::vector<RGBControllerInterface*> controllers = api->GetRGBControllers();

    for(RGBControllerInterface* ctrl : controllers)
    {
        std::string name = ctrl->GetName();
        if(name.find("Akko") != std::string::npos ||
           name.find("3108") != std::string::npos)
        {
            controller = ctrl;
            return;
        }
    }
}

int KeyboardEditorWidget::FindCustomModeIndex()
{
    if(controller == nullptr)
        return(-1);

    /* Device modes first (plugin virtual controller and the core
       driver both expose modes at DEVICE level, not zone level). */
    for(unsigned int m = 0; m < controller->GetModeCount(); m++)
    {
        if(controller->GetModeName(m) == "Custom")
            return((int)m);
    }

    /* Fallback: zone-level modes (pre-1.0 style controllers) */
    unsigned int nmodes = controller->GetZoneModeCount(0);

    for(unsigned int m = 0; m < nmodes; m++)
    {
        if(controller->GetZoneModeName(0, m) == "Custom")
            return((int)m);
    }

    return(-1);
}

/*---------------------------------------------------------*\
| Ensure the keyboard is in Custom (per-key buffer) mode    |
\*---------------------------------------------------------*/
void KeyboardEditorWidget::EnterCustomMode()
{
    if(controller == nullptr || custom_mode < 0)
        return;

    /* If already in Custom, nothing to do */
    if((int)controller->GetActiveMode() == custom_mode)
        return;

    /* Switch to Custom — triggers DeviceUpdateMode -> sends buffer */
    controller->SetActiveMode((unsigned int)custom_mode);

    /* Also update the zone selector in the Devices page so the  */
    /* combo-box reflects the change (SetZoneActiveMode only      */
    /* updates the GUI state without re-sending the mode).        */
    controller->SetZoneActiveMode(0, (unsigned int)custom_mode);
}

void KeyboardEditorWidget::SendBuffer()
{
    if(controller == nullptr)
        return;

    controller->UpdateZoneLEDs(0);
}

/*---------------------------------------------------------*\
| Refresh key_colors from the RGBController buffer          |
\*---------------------------------------------------------*/
void KeyboardEditorWidget::RefreshColors()
{
    if(controller == nullptr)
        return;

    unsigned int led_count = controller->GetLEDCount();
    if(led_count == 0)
        return;

    key_colors.resize(led_count);

    RGBColor* colors = controller->GetColorsPointer();
    if(colors == nullptr)
        return;

    for(unsigned int i = 0; i < led_count; i++)
    {
        key_colors[i] = RGBColorToQt(colors[i]);
    }

    canvas->SetKeyColors(key_colors);
}

/*---------------------------------------------------------*\
| User clicks a key                                         |
\*---------------------------------------------------------*/
void KeyboardEditorWidget::OnKeyClicked(int led_index)
{
    if(controller == nullptr)
    {
        FindController();
        if(controller == nullptr)
        {
            UpdateStatus("Tastiera Akko non trovata", false);
            return;
        }
        custom_mode = FindCustomModeIndex();
        found_flag = true;
    }

    if((unsigned int)led_index >= controller->GetLEDCount())
        return;

    /* Switch to Custom mode (sends the full buffer) */
    EnterCustomMode();

    /* Set the single LED color */
    RGBColor rgb = QtToRGBColor(current_color);
    controller->SetColor((unsigned int)led_index, rgb);
    key_colors[led_index] = current_color;

    /* Re-send the whole buffer to show the change */
    SendBuffer();
    canvas->SetKeyColors(key_colors);
    canvas->update();

    QString msg = QString("LED %1 impostato su %2 — %3")
                      .arg(led_index)
                      .arg(current_color.name())
                      .arg(QString::fromUtf8(akko_key_layout[led_index].label));
    UpdateStatus(msg, true);
}

/*---------------------------------------------------------*\
| Clear all LEDs (black buffer)                             |
\*---------------------------------------------------------*/
void KeyboardEditorWidget::OnClearAll()
{
    if(controller == nullptr)
        return;

    EnterCustomMode();

    unsigned int count = controller->GetLEDCount();
    for(unsigned int i = 0; i < count; i++)
    {
        controller->SetColor(i, 0x000000);
    }

    SendBuffer();
    RefreshColors();
    UpdateStatus("Tutti i LED impostati a nero", true);
}

/*---------------------------------------------------------*\
| Color picker                                              |
\*---------------------------------------------------------*/
void KeyboardEditorWidget::OnPickColor()
{
    QColor c = QColorDialog::getColor(current_color, this, "Scegli colore tasto",
                                      QColorDialog::ShowAlphaChannel);
    if(c.isValid())
        SetCurrentColor(c);
}

void KeyboardEditorWidget::SetCurrentColor(const QColor& c)
{
    current_color = c;

    /* Aggiorna il quadretto colore corrente */
    QPalette pal = current_color_label->palette();
    pal.setColor(QPalette::Window, c);
    current_color_label->setPalette(pal);

    /* Highlight relativo allo swatch selezionato */
    for(QPushButton* btn : swatch_buttons)
    {
        QString bg = btn->toolTip();
        bool same = (bg == c.name());
        btn->setStyleSheet(
            QString("QPushButton{ background-color:%1; border:2px solid %2; border-radius:3px; }"
                    "QPushButton:pressed{ border:2px solid #fff; }")
                .arg(bg, same ? "#fff" : "#555"));
    }
}

void KeyboardEditorWidget::OnLoadFromKeyboard()
{
    RefreshColors();
    canvas->SetKeyColors(key_colors);
    canvas->update();
    UpdateStatus("Buffer OpenRGB riletto dalla tastiera", true);
}

void KeyboardEditorWidget::OnToggleDirectMode(bool /*checked*/)
{
    /* Placeholder — per la Akko 3108 V2 non serve una direct mode.  */
    /* Il buffer viene aggiornato ad ogni clic sufficientemente.     */
    /* Questo bottone serve solo come promemoria futuro.              */
    direct_mode_btn->setText(direct_mode_btn->isChecked() ? "Direct ON" : "Direct OFF");
}

void KeyboardEditorWidget::UpdateStatus(const QString& msg, bool ok)
{
    status_label->setText(msg);
    status_label->setStyleSheet(ok ? "color:#8f8;" : "color:#f88;");
}