#include "settings.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QTemporaryDir>
#include <QtTest>

class SettingsTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
    KConfigGroup group() const
    {
        return KConfigGroup(KSharedConfig::openConfig(QStringLiteral("kwinrc")),
                            QStringLiteral("Effect-pulsepointer"));
    }
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
    }
    void init()
    {
        auto config = group();
        config.deleteGroup();
        config.sync();
    }
    void locatorIsOptionalAndPersists()
    {
        QVERIFY(!PulsePointer::Settings::defaults().hotspotLocator);
        QVERIFY(!PulsePointer::Settings::load().hotspotLocator);
        auto settings = PulsePointer::Settings::load();
        settings.hotspotLocator = true;
        settings.save();
        QCOMPARE(PulsePointer::Settings::load(), settings);
        settings.hotspotLocator = false;
        settings.save();
        QCOMPARE(PulsePointer::Settings::load(), settings);
    }
    void legacyModesMigrateWithoutAddingLayers_data()
    {
        QTest::addColumn<int>("legacy");
        for (int mode = 0; mode <= 6; ++mode) QTest::newRow(qPrintable(QString::number(mode))) << mode;
    }
    void legacyModesMigrateWithoutAddingLayers()
    {
        QFETCH(int, legacy);
        auto config = group();
        config.writeEntry("Mode", legacy);
        config.writeEntry("Intensity", 135);
        config.writeEntry("BeatRing", true);
        config.sync();
        const auto settings = PulsePointer::Settings::load();
        QCOMPARE(settings.intensity, 135);
        QVERIFY(!settings.hasScreenEffects());
        QCOMPARE(settings.pointerVisualizations,
                 PulsePointer::pointerBit(static_cast<PulsePointer::VisualizationMode>(legacy)));
        QCOMPARE(settings.cursor, legacy == 0 ? PulsePointer::CursorAnimation::Elastic
            : legacy == 6 ? PulsePointer::CursorAnimation::Wobble : PulsePointer::CursorAnimation::None);
        QCOMPARE(settings.beatRing, legacy == 0 || legacy == 6);
        settings.save();
        QCOMPARE(PulsePointer::Settings::load(), settings);
    }
    void allLayersPersistTogetherAndOverrideLegacyMode()
    {
        auto config = group();
        config.writeEntry("Mode", 4);
        config.sync();
        auto settings = PulsePointer::Settings::defaults();
        settings.cursor = PulsePointer::CursorAnimation::Wobble;
        settings.pointerVisualizations = PulsePointer::pointerVisualizationMask;
        settings.screenEffects.fill(73);
        settings.attackMs = 70;
        settings.releaseMs = 950;
        settings.monitors = PulsePointer::MonitorScope::Pointer;
        settings.edgeReach = 180;
        settings.edgeDensity = 30;
        settings.edgeSoftness = 15;
        settings.edgeProfile = 4;
        settings.screenIntensity = 120;
        settings.save();
        QCOMPARE(PulsePointer::Settings::load(), settings);
        settings.enabled = false;
        settings.save();
        QCOMPARE(PulsePointer::Settings::load(), settings);
    }
    void corruptConfigurationIsBounded()
    {
        auto config = group();
        config.writeEntry("CursorAnimation", 100);
        config.writeEntry("PointerVisualizations", quint32(0xffffffff));
        config.writeEntry("EdgeReach", -100);
        config.writeEntry("EdgeDensity", 900);
        config.writeEntry("AttackMs", 0);
        config.writeEntry("ReleaseMs", 99999);
        config.writeEntry("Monitors", -1);
        config.writeEntry("BreathingFrame", 1000);
        config.writeEntry("Gravity", -5);
        config.sync();
        const auto settings = PulsePointer::Settings::load();
        QCOMPARE(settings.cursor, PulsePointer::CursorAnimation::Wobble);
        QCOMPARE(settings.pointerVisualizations, PulsePointer::pointerVisualizationMask);
        QCOMPARE(settings.edgeReach, 16);
        QCOMPARE(settings.edgeDensity, 64);
        QCOMPARE(settings.attackMs, 10);
        QCOMPARE(settings.releaseMs, 2000);
        QCOMPARE(settings.monitors, PulsePointer::MonitorScope::All);
        QCOMPARE(settings.strength(PulsePointer::ScreenEffect::BreathingFrame), 100);
        QCOMPARE(settings.strength(PulsePointer::ScreenEffect::Gravity), 0);
    }
    void presetsReplaceEffectsAndKeepDisplayPreference()
    {
        auto settings = PulsePointer::Settings::defaults();
        settings.monitors = PulsePointer::MonitorScope::Pointer;
        settings.screenEffects.fill(100);
        settings.applyPreset(PulsePointer::Preset::Velvet);
        QCOMPARE(settings.strength(PulsePointer::ScreenEffect::Gravity), 0);
        QCOMPARE(settings.strength(PulsePointer::ScreenEffect::BreathingFrame), 35);
        QCOMPARE(settings.monitors, PulsePointer::MonitorScope::Pointer);
        for (int preset = 0; preset < 5; ++preset) {
            settings.applyPreset(static_cast<PulsePointer::Preset>(preset));
            QVERIFY(settings.hasScreenEffects());
            settings.save();
            QCOMPARE(PulsePointer::Settings::load(), settings);
        }
    }
};
QTEST_GUILESS_MAIN(SettingsTest)
#include "settings_test.moc"
