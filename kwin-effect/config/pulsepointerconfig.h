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

    QComboBox *m_mode = nullptr;
    QSlider *m_intensity = nullptr;
    QLabel *m_intensityValue = nullptr;
    QCheckBox *m_beatRing = nullptr;
    QPushButton *m_color = nullptr;
    QSpinBox *m_overlaySize = nullptr;
    QSlider *m_responsiveness = nullptr;
    QLabel *m_responsivenessValue = nullptr;
    PulsePointer::Settings m_loadedSettings;
    quint32 m_selectedColor = 0;
    bool m_loading = false;
};
