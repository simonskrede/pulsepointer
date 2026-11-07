#include "config/pulsepointerconfig.h"
#include <KConfigGroup>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

class ConfigTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
    }
    void independentLayersRoundTripThroughDialog()
    {
        auto original = PulsePointer::Settings::defaults();
        original.save();
        PulsePointerConfig module(nullptr, KPluginMetaData());
        auto *page = module.widget();
        auto *cursor = page->findChild<QComboBox *>(QStringLiteral("cursorAnimation"));
        auto *spectrum = page->findChild<QCheckBox *>(QStringLiteral("pointerLayer2"));
        auto *border = page->findChild<QCheckBox *>(QStringLiteral("effect_LiquidBorder"));
        auto *strength = page->findChild<QSpinBox *>(QStringLiteral("strength_LiquidBorder"));
        QVERIFY(cursor && spectrum && border && strength);
        QVERIFY(!strength->isEnabled());
        cursor->setCurrentIndex(cursor->findData(int(PulsePointer::CursorAnimation::Wobble)));
        spectrum->setChecked(true);
        border->setChecked(true);
        QVERIFY(strength->isEnabled());
        strength->setValue(67);
        module.save();
        auto saved = PulsePointer::Settings::load();
        QCOMPARE(saved.cursor, PulsePointer::CursorAnimation::Wobble);
        QVERIFY(saved.pointerVisualizations & PulsePointer::pointerBit(PulsePointer::VisualizationMode::Spectrum));
        QCOMPARE(saved.strength(PulsePointer::ScreenEffect::LiquidBorder), 67);
        module.load();
        QVERIFY(spectrum->isChecked());
        QVERIFY(border->isChecked());
        QCOMPARE(strength->value(), 67);
        border->setChecked(false);
        module.save();
        saved = PulsePointer::Settings::load();
        QCOMPARE(saved.strength(PulsePointer::ScreenEffect::LiquidBorder), 0);
        QCOMPARE(saved.cursor, PulsePointer::CursorAnimation::Wobble);
        QVERIFY(saved.pointerVisualizations != 0);
        module.defaults();
        module.save();
        QCOMPARE(PulsePointer::Settings::load(), PulsePointer::Settings::defaults());
    }
    void hotspotAvailableInEveryCursorMode()
    {
        PulsePointer::Settings::defaults().save();
        PulsePointerConfig module(nullptr, KPluginMetaData());
        auto *cursor = module.widget()->findChild<QComboBox *>(QStringLiteral("cursorAnimation"));
        auto *locator = module.widget()->findChild<QCheckBox *>(QStringLiteral("hotspotLocator"));
        QVERIFY(cursor && locator);
        for (const auto mode : {PulsePointer::CursorAnimation::Elastic, PulsePointer::CursorAnimation::Wobble,
                                PulsePointer::CursorAnimation::None}) {
            cursor->setCurrentIndex(cursor->findData(int(mode)));
            QVERIFY(locator->isEnabled());
            locator->setChecked(true);
            module.save();
            module.load();
            QVERIFY(locator->isEnabled());
            QVERIFY(locator->isChecked());
            const auto saved = PulsePointer::Settings::load();
            QCOMPARE(saved.cursor, mode);
            QVERIFY(saved.hotspotLocator);
        }
    }
    void captureSettingsPages()
    {
        const auto artifacts = qEnvironmentVariable("PULSEPOINTER_TEST_ARTIFACTS");
        if (artifacts.isEmpty()) return;
        PulsePointerConfig module(nullptr, KPluginMetaData());
        auto *page = module.widget();
        page->resize(820, 720);
        page->show();
        auto *tabs = page->findChild<QTabWidget *>();
        QVERIFY(tabs);
        QDir().mkpath(artifacts);
        for (int i = 0; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            QApplication::processEvents();
            QVERIFY(page->grab().save(artifacts + QStringLiteral("/settings-%1.png").arg(i)));
        }
    }
};
QTEST_MAIN(ConfigTest)
#include "config_test.moc"
