#include "pulsepointerconfig.h"

#include <KPluginFactory>

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QScrollArea>
#include <QTabWidget>
#include <QGroupBox>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

QWidget *sliderRow(QSlider *slider, QLabel *value, QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(slider, 1);
    value->setMinimumWidth(48);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(value);
    return container;
}

QString percentage(int value)
{
    return QStringLiteral("%1%").arg(value);
}

} // namespace

PulsePointerConfig::PulsePointerConfig(QObject *parent, const KPluginMetaData &metadata)
    : KCModule(parent, metadata)
{
    auto *outerLayout = new QVBoxLayout(widget());
    auto *intro = new QLabel(tr("Combine any cursor, border, color, and atmosphere effects. "
                               "Unchecked effects are off; each checked effect has its own strength."), widget());
    intro->setWordWrap(true);
    outerLayout->addWidget(intro);
    m_enabled = new QCheckBox(tr("Enable music visualization"), widget());
    outerLayout->addWidget(m_enabled);
    connect(m_enabled, &QCheckBox::toggled, this, &PulsePointerConfig::updateChangeState);

    auto *presetRow = new QHBoxLayout;
    auto *presets = new QComboBox(widget());
    presets->addItems({tr("Velvet — soft glow and color"), tr("Voltage — sharp edges and sparks"),
                       tr("Underwater — liquid motion"), tr("Aurora — ribbons and particles"),
                       tr("Meltdown — distortion and echoes")});
    auto *applyPreset = new QPushButton(tr("Use preset"), widget());
    presetRow->addWidget(presets, 1);
    presetRow->addWidget(applyPreset);
    outerLayout->addLayout(presetRow);
    connect(applyPreset, &QPushButton::clicked, this, [this, presets] {
        auto settings = settingsFromWidgets();
        settings.applyPreset(static_cast<PulsePointer::Preset>(presets->currentIndex()));
        setWidgets(settings);
        updateChangeState();
    });

    auto *accentRow = new QFormLayout;
    m_color = new QPushButton(widget());
    m_color->setToolTip(tr("Shared tint for pointer visualizers and decorative screen effects. "
                           "Desktop hue and saturation effects change the desktop's existing colors."));
    accentRow->addRow(tr("Shared accent color:"), m_color);
    outerLayout->addLayout(accentRow);
    auto *tabs = new QTabWidget(widget());
    outerLayout->addWidget(tabs, 1);
    auto makePage = [tabs](const QString &name) {
        auto *scroll = new QScrollArea(tabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *page = new QWidget(scroll);
        scroll->setWidget(page);
        tabs->addTab(scroll, name);
        return page;
    };
    auto *cursorPage = makePage(tr("Mouse pointer"));
    auto *cursorLayout = new QVBoxLayout(cursorPage);
    auto *form = new QFormLayout;
    cursorLayout->addLayout(form);
    cursorLayout->addStretch();
    m_cursor = new QComboBox(widget());
    m_cursor->setObjectName(QStringLiteral("cursorAnimation"));
    m_cursor->addItem(tr("Unchanged"), int(PulsePointer::CursorAnimation::None));
    m_cursor->addItem(tr("Elastic — stretch and squash"), int(PulsePointer::CursorAnimation::Elastic));
    m_cursor->addItem(tr("Wobble — swing and bounce"), int(PulsePointer::CursorAnimation::Wobble));
    form->addRow(tr("Cursor animation:"), m_cursor);
    auto *pointerGroup = new QGroupBox(tr("Visualizers around the pointer — combine freely"), widget());
    auto *pointerLayout = new QVBoxLayout(pointerGroup);
    const std::array<QString, 5> pointerNames{tr("Level bar"), tr("Waveform circle"),
        tr("Spectrum bars"), tr("Oscilloscope"), tr("Spectrum halo")};
    for (std::size_t i = 0; i < m_pointerLayers.size(); ++i) {
        m_pointerLayers[i] = new QCheckBox(pointerNames[i], pointerGroup);
        m_pointerLayers[i]->setObjectName(QStringLiteral("pointerLayer%1").arg(i));
        pointerLayout->addWidget(m_pointerLayers[i]);
        connect(m_pointerLayers[i], &QCheckBox::toggled, this, [this] {
            updateControlAvailability();
            updateChangeState();
        });
    }
    form->addRow(pointerGroup);
    m_intensity = new QSlider(Qt::Horizontal, widget());
    m_intensity->setRange(0, 200);
    m_intensityValue = new QLabel(widget());
    form->addRow(tr("Pointer intensity:"), sliderRow(m_intensity, m_intensityValue, widget()));

    m_responsiveness = new QSlider(Qt::Horizontal, widget());
    m_responsiveness->setRange(50, 200);
    m_responsivenessValue = new QLabel(widget());
    form->addRow(tr("Cursor responsiveness:"),
                 sliderRow(m_responsiveness, m_responsivenessValue, widget()));

    m_beatRing = new QCheckBox(tr("Show a ring around the pointer on musical attacks"), widget());
    form->addRow(QString(), m_beatRing);

    m_hotspotLocator = new QCheckBox(tr("Show a red marker at the cursor hotspot"), widget());
    m_hotspotLocator->setObjectName(QStringLiteral("hotspotLocator"));
    m_hotspotLocator->setToolTip(tr("Mark the exact click position with a small red circle. "
                                   "The marker follows display scaling and stays the same size during cursor animation. "
                                   "Works with Unchanged, Elastic, and Wobble cursors."));
    form->addRow(QString(), m_hotspotLocator);

    m_overlaySize = new QSpinBox(widget());
    m_overlaySize->setRange(16, 128);
    m_overlaySize->setSuffix(tr(" px"));
    form->addRow(tr("Pointer visualizer size:"), m_overlaySize);

    auto spin = [this](int low, int high, const QString &suffix) {
        auto *control = new QSpinBox(widget());
        control->setRange(low, high);
        control->setSuffix(suffix);
        connect(control, &QSpinBox::valueChanged, this, &PulsePointerConfig::updateChangeState);
        return control;
    };
    auto addEffects = [this](QVBoxLayout *layout, std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const auto &info = PulsePointer::screenEffectCatalog[i];
            auto *row = new QHBoxLayout;
            auto *text = new QVBoxLayout;
            m_effectChecks[i] = new QCheckBox(tr(info.name), widget());
            m_effectChecks[i]->setObjectName(QStringLiteral("effect_%1").arg(QString::fromLatin1(info.key)));
            auto *description = new QLabel(tr(info.description), widget());
            description->setWordWrap(true);
            text->addWidget(m_effectChecks[i]);
            text->addWidget(description);
            row->addLayout(text, 1);
            auto *strength = new QSpinBox(widget());
            strength->setObjectName(QStringLiteral("strength_%1").arg(QString::fromLatin1(info.key)));
            strength->setRange(1, 100);
            strength->setSuffix(tr("%"));
            strength->setAccessibleName(tr("%1 strength").arg(tr(info.name)));
            strength->setToolTip(tr("Strength of this effect. Uncheck its name to turn it off."));
            m_effectStrengths[i] = strength;
            row->addWidget(strength);
            layout->addLayout(row);
            connect(m_effectChecks[i], &QCheckBox::toggled, this, [this, i](bool on) {
                m_effectStrengths[i]->setEnabled(on);
                updateChangeState();
            });
            connect(strength, &QSpinBox::valueChanged, this, &PulsePointerConfig::updateChangeState);
        }
    };
    auto *borderLayout = new QVBoxLayout(makePage(tr("Screen borders")));
    addEffects(borderLayout, 0, 6);
    auto *borderForm = new QFormLayout;
    m_edgeReach = spin(16, 400, tr(" px"));
    m_edgeReach->setToolTip(tr("Maximum depth of the border into the screen, in logical pixels."));
    m_edgeDensity = spin(4, 64, QString());
    m_edgeDensity->setToolTip(tr("Number of waves or teeth along each edge."));
    m_edgeSoftness = spin(0, 100, tr("%"));
    m_edgeSoftness->setToolTip(tr("0% gives crisp edges; 100% gives soft, feathered edges."));
    m_edgeProfile = new QComboBox(widget());
    m_edgeProfile->addItems({tr("Natural for each effect"), tr("Smooth sine"), tr("Rounded scallops"),
                            tr("Triangles"), tr("Narrow spikes")});
    m_edgeProfile->setToolTip(tr("Wave shape used by Razor frame and Liquid border."));
    connect(m_edgeProfile, &QComboBox::currentIndexChanged, this, &PulsePointerConfig::updateChangeState);
    borderForm->addRow(tr("Border profile:"), m_edgeProfile);
    borderForm->addRow(tr("Edge reach:"), m_edgeReach);
    borderForm->addRow(tr("Wave / tooth count:"), m_edgeDensity);
    borderForm->addRow(tr("Edge softness:"), m_edgeSoftness);
    borderLayout->addLayout(borderForm);
    borderLayout->addStretch();
    auto *colorLayout = new QVBoxLayout(makePage(tr("Desktop color")));
    addEffects(colorLayout, 6, 12);
    colorLayout->addStretch();
    auto *sceneLayout = new QVBoxLayout(makePage(tr("Atmosphere & distortion")));
    addEffects(sceneLayout, 12, PulsePointer::screenEffectCount);
    sceneLayout->addStretch();

    auto *generalLayout = new QVBoxLayout(makePage(tr("Timing & displays")));
    auto *generalForm = new QFormLayout;
    m_screenIntensity = spin(0, 200, tr("%"));
    m_screenIntensity->setToolTip(tr("Scales all screen effects together. Pointer intensity is separate."));
    m_attack = spin(10, 500, tr(" ms"));
    m_attack->setToolTip(tr("How quickly screen effects grow when the music becomes louder. Lower is snappier."));
    m_release = spin(50, 2000, tr(" ms"));
    m_release->setToolTip(tr("How slowly screen effects fade after a note. Higher gives a longer afterglow."));
    m_monitors = new QComboBox(widget());
    m_monitors->addItem(tr("All displays"), int(PulsePointer::MonitorScope::All));
    m_monitors->addItem(tr("Only the display containing the pointer"), int(PulsePointer::MonitorScope::Pointer));
    generalForm->addRow(tr("Screen effect intensity:"), m_screenIntensity);
    generalForm->addRow(tr("Rise time:"), m_attack);
    generalForm->addRow(tr("Fade time:"), m_release);
    generalForm->addRow(tr("Screen effects appear on:"), m_monitors);
    generalLayout->addLayout(generalForm);
    auto *help = new QLabel(tr("Effects settle back to the normal desktop during silence and stop when audio "
                               "disconnects or the screen locks. Musical attacks are detected from the audio; "
                               "movement is not locked to a fixed tempo.\n\n"
                               "Quick bypass: Meta+Ctrl+Alt+P. Change this in System Settings → Shortcuts → KWin "
                               "→ Toggle PulsePointer. The shortcut temporarily pauses all layers."), widget());
    help->setWordWrap(true);
    generalLayout->addWidget(help);
    auto *clear = new QPushButton(tr("Turn off all screen effects"), widget());
    generalLayout->addWidget(clear);
    connect(clear, &QPushButton::clicked, this, [this] {
        for (auto *check : m_effectChecks) {
            check->setChecked(false);
        }
    });
    generalLayout->addStretch();
    connect(m_monitors, &QComboBox::currentIndexChanged, this, &PulsePointerConfig::updateChangeState);
    connect(m_cursor, &QComboBox::currentIndexChanged, this, [this] {
        updateControlAvailability();
        updateChangeState();
    });
    connect(m_intensity, &QSlider::valueChanged, this, [this](int value) {
        m_intensityValue->setText(percentage(value));
        updateChangeState();
    });
    connect(m_responsiveness, &QSlider::valueChanged, this, [this](int value) {
        m_responsivenessValue->setText(percentage(value));
        updateChangeState();
    });
    connect(m_beatRing, &QCheckBox::toggled, this, &PulsePointerConfig::updateChangeState);
    connect(m_hotspotLocator, &QCheckBox::toggled, this, &PulsePointerConfig::updateChangeState);
    connect(m_overlaySize, &QSpinBox::valueChanged, this, &PulsePointerConfig::updateChangeState);
    connect(m_color, &QPushButton::clicked, this, &PulsePointerConfig::chooseColor);

    load();
}

void PulsePointerConfig::load()
{
    m_loadedSettings = PulsePointer::Settings::load();
    setWidgets(m_loadedSettings);
    setNeedsSave(false);
    setRepresentsDefaults(m_loadedSettings == PulsePointer::Settings::defaults());
}

void PulsePointerConfig::save()
{
    m_loadedSettings = settingsFromWidgets();
    m_loadedSettings.save();

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect"));
    message << QStringLiteral("pulsepointer");
    QDBusConnection::sessionBus().asyncCall(message);

    setNeedsSave(false);
    setRepresentsDefaults(m_loadedSettings == PulsePointer::Settings::defaults());
}

void PulsePointerConfig::defaults()
{
    setWidgets(PulsePointer::Settings::defaults());
    updateChangeState();
}

PulsePointer::Settings PulsePointerConfig::settingsFromWidgets() const
{
    PulsePointer::Settings settings;
    settings.enabled = m_enabled->isChecked();
    settings.cursor = static_cast<PulsePointer::CursorAnimation>(m_cursor->currentData().toInt());
    settings.pointerVisualizations = 0;
    for (std::size_t i = 0; i < m_pointerLayers.size(); ++i) {
        if (m_pointerLayers[i]->isChecked()) {
            settings.pointerVisualizations |= quint32(1) << i;
        }
    }
    for (std::size_t i = 0; i < m_effectChecks.size(); ++i) {
        settings.screenEffects[i] = m_effectChecks[i]->isChecked() ? m_effectStrengths[i]->value() : 0;
    }
    settings.screenIntensity = m_screenIntensity->value();
    settings.edgeReach = m_edgeReach->value();
    settings.edgeDensity = m_edgeDensity->value();
    settings.edgeSoftness = m_edgeSoftness->value();
    settings.edgeProfile = m_edgeProfile->currentIndex();
    settings.attackMs = m_attack->value();
    settings.releaseMs = m_release->value();
    settings.monitors = static_cast<PulsePointer::MonitorScope>(m_monitors->currentData().toInt());
    settings.intensity = m_intensity->value();
    settings.beatRing = m_beatRing->isChecked();
    settings.hotspotLocator = m_hotspotLocator->isChecked();
    settings.color = m_selectedColor;
    settings.overlaySize = m_overlaySize->value();
    settings.responsiveness = m_responsiveness->value();
    return settings;
}

void PulsePointerConfig::setWidgets(const PulsePointer::Settings &settings)
{
    m_loading = true;
    m_enabled->setChecked(settings.enabled);
    m_cursor->setCurrentIndex(m_cursor->findData(int(settings.cursor)));
    for (std::size_t i = 0; i < m_pointerLayers.size(); ++i) {
        m_pointerLayers[i]->setChecked(settings.pointerVisualizations & (quint32(1) << i));
    }
    for (std::size_t i = 0; i < m_effectChecks.size(); ++i) {
        m_effectChecks[i]->setChecked(settings.screenEffects[i] > 0);
        m_effectStrengths[i]->setValue(settings.screenEffects[i] > 0 ? settings.screenEffects[i] : 50);
        m_effectStrengths[i]->setEnabled(settings.screenEffects[i] > 0);
    }
    m_screenIntensity->setValue(settings.screenIntensity);
    m_edgeReach->setValue(settings.edgeReach);
    m_edgeDensity->setValue(settings.edgeDensity);
    m_edgeSoftness->setValue(settings.edgeSoftness);
    m_edgeProfile->setCurrentIndex(settings.edgeProfile);
    m_attack->setValue(settings.attackMs);
    m_release->setValue(settings.releaseMs);
    m_monitors->setCurrentIndex(m_monitors->findData(int(settings.monitors)));
    m_intensity->setValue(settings.intensity);
    m_beatRing->setChecked(settings.beatRing);
    m_hotspotLocator->setChecked(settings.hotspotLocator);
    m_selectedColor = settings.color;
    m_overlaySize->setValue(settings.overlaySize);
    m_responsiveness->setValue(settings.responsiveness);
    m_intensityValue->setText(percentage(settings.intensity));
    m_responsivenessValue->setText(percentage(settings.responsiveness));
    updateColorButton();
    updateControlAvailability();
    m_loading = false;
}

void PulsePointerConfig::chooseColor()
{
    const QColor selected = QColorDialog::getColor(
        QColor::fromRgba(m_selectedColor),
        widget(),
        tr("Choose visualization color"),
        QColorDialog::ShowAlphaChannel);
    if (!selected.isValid()) {
        return;
    }
    m_selectedColor = selected.rgba();
    updateColorButton();
    updateChangeState();
}

void PulsePointerConfig::updateColorButton()
{
    const QColor color = QColor::fromRgba(m_selectedColor);
    m_color->setText(color.name(QColor::HexArgb).toUpper());
    m_color->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: rgba(%1, %2, %3, %4); }")
                               .arg(color.red())
                               .arg(color.green())
                               .arg(color.blue())
                               .arg(color.alpha()));
}

void PulsePointerConfig::updateControlAvailability()
{
    const bool cursorMode = m_cursor->currentData().toInt() != int(PulsePointer::CursorAnimation::None);
    m_responsiveness->setEnabled(cursorMode);
    m_overlaySize->setEnabled(std::any_of(m_pointerLayers.begin(), m_pointerLayers.end(),
                                        [](QCheckBox *check) { return check->isChecked(); }));
}

void PulsePointerConfig::updateChangeState()
{
    if (m_loading) {
        return;
    }
    const PulsePointer::Settings current = settingsFromWidgets();
    setNeedsSave(current != m_loadedSettings);
    setRepresentsDefaults(current == PulsePointer::Settings::defaults());
}

K_PLUGIN_CLASS_WITH_JSON(PulsePointerConfig, "pulsepointer_config.json")

#include "pulsepointerconfig.moc"
