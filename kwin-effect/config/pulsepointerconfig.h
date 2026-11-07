#pragma once

#include "settings.h"

#include <KCModule>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

class PulsePointerConfig : public KCModule
{
    Q_OBJECT

public:
    PulsePointerConfig(QObject *parent, const KPluginMetaData &metadata);

    void load() override;
    void save() override;
    void defaults() override;

private:
    PulsePointer::Settings settingsFromWidgets() const;
    void setWidgets(const PulsePointer::Settings &settings);
    void chooseColor();
    void updateColorButton();
    void updateControlAvailability();
    void updateChangeState();

    QCheckBox *m_enabled = nullptr;
    QComboBox *m_cursor = nullptr;
    std::array<QCheckBox *, 5> m_pointerLayers{};
    std::array<QCheckBox *, PulsePointer::screenEffectCount> m_effectChecks{};
    std::array<QSpinBox *, PulsePointer::screenEffectCount> m_effectStrengths{};
    QSpinBox *m_screenIntensity = nullptr;
    QSpinBox *m_edgeReach = nullptr;
    QSpinBox *m_edgeDensity = nullptr;
    QSpinBox *m_edgeSoftness = nullptr;
    QComboBox *m_edgeProfile = nullptr;
    QSpinBox *m_attack = nullptr;
    QSpinBox *m_release = nullptr;
    QComboBox *m_monitors = nullptr;
    QSlider *m_intensity = nullptr;
    QLabel *m_intensityValue = nullptr;
    QCheckBox *m_beatRing = nullptr;
    QCheckBox *m_hotspotLocator = nullptr;
    QPushButton *m_color = nullptr;
    QSpinBox *m_overlaySize = nullptr;
    QSlider *m_responsiveness = nullptr;
    QLabel *m_responsivenessValue = nullptr;
    PulsePointer::Settings m_loadedSettings;
    quint32 m_selectedColor = 0;
    bool m_loading = false;
};
