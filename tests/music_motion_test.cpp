#include "musicmotion.h"
#include <QtTest>

class MusicMotionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void envelopesFollowElapsedTime()
    {
        PulsePointer::MusicMotion fast, slow;
        fast.audio(0.7, 0.8, 0.4, 0.2, 0.6);
        slow = fast;
        for (int i = 0; i < 100; ++i) fast.advance(0.01, 45, 350);
        for (int i = 0; i < 50; ++i) slow.advance(0.02, 45, 350);
        for (int i = 0; i < 4; ++i) QVERIFY(std::abs(fast.bands[i] - slow.bands[i]) < 0.0001);
        QVERIFY(std::abs(fast.pulse - slow.pulse) < 0.0001);
        QVERIFY(std::abs(fast.waveAge[0] - slow.waveAge[0]) < 0.0001);
    }
    void silenceSettlesAndStartupDoesNotCollapse()
    {
        PulsePointer::MusicMotion motion;
        QVERIFY(!motion.moving());
        for (int i = 0; i < 300; ++i) motion.advance(0.02, 45, 350);
        QCOMPARE(motion.collapse(), 0.0f);
        motion.audio(0.6, 0.7, 0.4, 0.2, 0.8);
        QVERIFY(motion.moving());
        for (int i = 0; i < 50; ++i) motion.advance(0.02, 45, 350);
        motion.audio(0, 0, 0, 0, 0);
        for (int i = 0; i < 100; ++i) motion.advance(0.02, 45, 350);
        QVERIFY(motion.collapse() > 0);
        for (int i = 0; i < 500; ++i) motion.advance(0.02, 45, 350);
        QVERIFY(!motion.moving());
        QVERIFY(motion.activity() < 0.0001);
        QCOMPARE(motion.collapse(), 0.0f);
    }
    void quietPassageThenReturningEnergyTriggersDrop()
    {
        PulsePointer::MusicMotion motion;
        motion.audio(0.5, 0.6, 0.3, 0.1, 0.6);
        motion.advance(0.02, 45, 350);
        motion.audio(0, 0, 0, 0, 0);
        for (int i = 0; i < 60; ++i) motion.advance(0.02, 45, 350);
        motion.audio(0.6, 0.8, 0.4, 0.2, 0.7);
        QCOMPARE(motion.dropAge, 0.0f);
        QVERIFY(motion.dropStrength > 0.5);
    }
    void sustainedOnsetDoesNotSpawnEveryPacket()
    {
        PulsePointer::MusicMotion motion;
        motion.audio(0.5, 0.6, 0.3, 0.1, 0.6);
        motion.advance(0.02, 45, 350);
        motion.audio(0.5, 0.6, 0.3, 0.1, 0.5);
        QCOMPARE(motion.nextWave, std::size_t(1));
        QVERIFY(motion.waveAge[0] > 0);
        motion.audio(0.5, 0.6, 0.3, 0.1, 0);
        motion.audio(0.5, 0.6, 0.3, 0.1, 0.4);
        QCOMPARE(motion.nextWave, std::size_t(2));
    }
};
QTEST_GUILESS_MAIN(MusicMotionTest)
#include "music_motion_test.moc"
