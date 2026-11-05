#include "settings.h"

#include <QTemporaryDir>
#include <QtTest>

class SettingsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void locatorIsOptionalAndPersists()
    {
        QTemporaryDir configDirectory;
        QVERIFY(configDirectory.isValid());
        qputenv("XDG_CONFIG_HOME", configDirectory.path().toUtf8());

        QVERIFY(!PulsePointer::Settings::defaults().hotspotLocator);
        QVERIFY(!PulsePointer::Settings::load().hotspotLocator);

        PulsePointer::Settings settings = PulsePointer::Settings::load();
        settings.hotspotLocator = true;
        settings.save();
        QCOMPARE(PulsePointer::Settings::load(), settings);

        settings.hotspotLocator = false;
        settings.save();
        QCOMPARE(PulsePointer::Settings::load(), settings);
    }
};

QTEST_GUILESS_MAIN(SettingsTest)

#include "settings_test.moc"
