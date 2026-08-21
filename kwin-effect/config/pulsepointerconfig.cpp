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
    auto *form = new QFormLayout;
    outerLayout->addLayout(form);
    outerLayout->addStretch();

    m_mode = new QComboBox(widget());
    m_mode->addItem(tr("Elastic cursor"), static_cast<int>(PulsePointer::VisualizationMode::Elastic));
    m_mode->addItem(tr("Wobble cursor"), static_cast<int>(PulsePointer::VisualizationMode::Wobble));
    m_mode->addItem(tr("Level bar"), static_cast<int>(PulsePointer::VisualizationMode::Bar));
    m_mode->addItem(tr("Waveform circle"), static_cast<int>(PulsePointer::VisualizationMode::Circle));
    m_mode->addItem(tr("Spectrum"), static_cast<int>(PulsePointer::VisualizationMode::Spectrum));
    m_mode->addItem(tr("Spectrum halo"), static_cast<int>(PulsePointer::VisualizationMode::SpectrumHalo));
    m_mode->addItem(tr("Oscilloscope"), static_cast<int>(PulsePointer::VisualizationMode::Oscilloscope));
    form->addRow(tr("Visualization:"), m_mode);

    m_intensity = new QSlider(Qt::Horizontal, widget());
    m_intensity->setRange(0, 200);
    m_intensityValue = new QLabel(widget());
    form->addRow(tr("Intensity:"), sliderRow(m_intensity, m_intensityValue, widget()));

    m_responsiveness = new QSlider(Qt::Horizontal, widget());
    m_responsiveness->setRange(50, 200);
    m_responsivenessValue = new QLabel(widget());
    form->addRow(tr("Responsiveness:"),
                 sliderRow(m_responsiveness, m_responsivenessValue, widget()));

    m_beatRing = new QCheckBox(tr("Show a ring on detected beats"), widget());
    form->addRow(QString(), m_beatRing);

    m_color = new QPushButton(widget());
    form->addRow(tr("Color:"), m_color);

    m_overlaySize = new QSpinBox(widget());
    m_overlaySize->setRange(16, 128);
    m_overlaySize->setSuffix(tr(" px"));
    form->addRow(tr("Overlay size:"), m_overlaySize);

    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] {
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
    settings.mode = static_cast<PulsePointer::VisualizationMode>(m_mode->currentData().toInt());
    settings.intensity = m_intensity->value();
    settings.beatRing = m_beatRing->isChecked();
    settings.color = m_selectedColor;
    settings.overlaySize = m_overlaySize->value();
    settings.responsiveness = m_responsiveness->value();
    return settings;
}

void PulsePointerConfig::setWidgets(const PulsePointer::Settings &settings)
{
    m_loading = true;
    const int index = m_mode->findData(static_cast<int>(settings.mode));
    m_mode->setCurrentIndex(std::max(0, index));
    m_intensity->setValue(settings.intensity);
    m_beatRing->setChecked(settings.beatRing);
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
    const auto mode = static_cast<PulsePointer::VisualizationMode>(m_mode->currentData().toInt());
    const bool cursorMode = mode == PulsePointer::VisualizationMode::Elastic
        || mode == PulsePointer::VisualizationMode::Wobble;
    m_beatRing->setEnabled(cursorMode);
    m_responsiveness->setEnabled(cursorMode);
    m_overlaySize->setEnabled(!cursorMode);
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
