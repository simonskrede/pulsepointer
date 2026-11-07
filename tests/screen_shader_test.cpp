#include "effectcatalog.h"

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QtTest>
#include <cmath>
#include <vector>

// Exercise the shipped shader with Mesa's surfaceless EGL backend, without a desktop.
class ScreenShaderTest : public QObject
{
    Q_OBJECT
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    GLuint program = 0, framebuffer = 0, target = 0, scene = 0, history = 0, audio = 0, vao = 0, vbo = 0;
    static constexpr int width = 320, height = 180;
    QImage input;
    QString error;

    GLuint compile(GLenum type, const char *path)
    {
        QFile file(QString::fromLatin1(path));
        if (!file.open(QIODevice::ReadOnly)) {
            error = file.errorString();
            return 0;
        }
        const QByteArray prefix = qEnvironmentVariableIsSet("PULSEPOINTER_SHADER_GLES")
            ? QByteArrayLiteral("#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n")
            : QByteArrayLiteral("#version 140\n");
        const QByteArray source = prefix + file.readAll();
        const char *text = source.constData();
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[8192];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            error = QString::fromUtf8(log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
    GLuint texture(int w, int h, GLenum format, GLenum type, const void *data)
    {
        GLuint result;
        glGenTextures(1, &result);
        glBindTexture(GL_TEXTURE_2D, result);
        glTexImage2D(GL_TEXTURE_2D, 0, format == GL_RED ? GL_R32F : GL_RGBA8,
                     w, h, 0, format, type, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return result;
    }
    void scalar(const char *name, float value) { glUniform1f(glGetUniformLocation(program, name), value); }
    void vec4(const char *name, float a, float b, float c, float d)
    {
        glUniform4f(glGetUniformLocation(program, name), a, b, c, d);
    }
    void setMusic(bool playing)
    {
        vec4("bands", playing ? 0.6 : 0, playing ? 0.75 : 0, playing ? 0.4 : 0, playing ? 0.5 : 0);
        vec4("waveAge", playing ? 0.24 : 10, playing ? 0.6 : 10, 10, 10);
        vec4("waveStrength", playing ? 0.85 : 0, playing ? 0.5 : 0, 0, 0);
        scalar("pulse", playing ? 0.75 : 0);
        scalar("activity", playing ? 1 : 0);
        scalar("collapse", 0);
        scalar("dropAge", playing ? 0.035 : 10);
        scalar("dropStrength", playing ? 0.8 : 0);
        scalar("historyValid", playing ? 1 : 0);
    }
    QImage render(const std::array<int, PulsePointer::screenEffectCount> &strengths)
    {
        for (std::size_t i = 0; i < strengths.size() / 4; ++i) {
            const QByteArray name = QByteArrayLiteral("fx") + QByteArray::number(i);
            vec4(name.constData(), strengths[i * 4] / 100.0, strengths[i * 4 + 1] / 100.0,
                 strengths[i * 4 + 2] / 100.0, strengths[i * 4 + 3] / 100.0);
        }
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        QImage result(width, height, QImage::Format_RGBA8888);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.bits());
        return result;
    }
    double difference(const QImage &a, const QImage &b)
    {
        double sum = 0;
        for (int i = 0; i < a.sizeInBytes(); ++i) sum += std::abs(int(a.constBits()[i]) - int(b.constBits()[i]));
        return sum / a.sizeInBytes();
    }
private Q_SLOTS:
    void initTestCase()
    {
        const auto platformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (!platformDisplay) QSKIP("EGL platform display extension is unavailable");
        display = platformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        EGLint major, minor;
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) {
            QSKIP("Surfaceless EGL is unavailable");
        }
        const bool gles = qEnvironmentVariableIsSet("PULSEPOINTER_SHADER_GLES");
        QVERIFY(eglBindAPI(gles ? EGL_OPENGL_ES_API : EGL_OPENGL_API));
        const EGLint configAttributes[]{EGL_RENDERABLE_TYPE, gles ? EGL_OPENGL_ES3_BIT_KHR : EGL_OPENGL_BIT,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
        EGLConfig config;
        EGLint count;
        QVERIFY(eglChooseConfig(display, configAttributes, &config, 1, &count));
        QVERIFY(count > 0);
        const EGLint contextAttributes[]{EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, gles ? 0 : 1, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        QVERIFY(context != EGL_NO_CONTEXT);
        QVERIFY(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
        qInfo("Shader test renderer: %s", glGetString(GL_RENDERER));
        const GLuint vertex = compile(GL_VERTEX_SHADER, ":/pulsepointer/shaders/screen.vert");
        QVERIFY2(vertex, qPrintable(error));
        const GLuint fragment = compile(GL_FRAGMENT_SHADER, ":/pulsepointer/shaders/screen.frag");
        QVERIFY2(fragment, qPrintable(error));
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glBindAttribLocation(program, 0, "vertex");
        glBindAttribLocation(program, 1, "texCoord");
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[8192];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            QFAIL(log);
        }
        glUseProgram(program);
        input = QImage(width, height, QImage::Format_RGBA8888);
        QImage old(width, height, QImage::Format_RGBA8888);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const QColor color((x * 5 + y * 2) % 256, (x / 32 % 2) ? 200 : 30, y * 255 / height);
                input.setPixelColor(x, y, color);
                old.setPixelColor(x, y, QColor(color.blue(), color.red(), color.green()));
            }
        }
        scene = texture(width, height, GL_RGBA, GL_UNSIGNED_BYTE, input.constBits());
        history = texture(width, height, GL_RGBA, GL_UNSIGNED_BYTE, old.constBits());
        std::array<float, 160> samples;
        for (int i = 0; i < 32; ++i) samples[i] = 0.3 + 0.25 * std::sin(i * 0.3);
        for (int i = 0; i < 128; ++i) samples[32 + i] = std::sin(i * 0.2) * 0.8;
        audio = texture(160, 1, GL_RED, GL_FLOAT, samples.data());
        target = texture(width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
        QCOMPARE(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
        for (int i = 0; i < 3; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, i == 0 ? scene : i == 1 ? history : audio);
            glUniform1i(glGetUniformLocation(program, i == 0 ? "scene" : i == 1 ? "history" : "audioData"), i);
        }
        const float vertices[]{-1,-1,0,0, -1,1,0,1, 1,-1,1,0, 1,1,1,1};
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));
        const float identity[]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        glUniformMatrix4fv(glGetUniformLocation(program, "modelViewProjectionMatrix"), 1, GL_FALSE, identity);
        glUniform2f(glGetUniformLocation(program, "size"), width, height);
        glUniform2f(glGetUniformLocation(program, "pointer"), 0.4, 0.55);
        vec4("tint", 1, 0.3, 0.08, 0.8);
        scalar("reach", 50);
        scalar("density", 12);
        scalar("softness", 0.4);
        scalar("time", 0.7);
        scalar("hue", 0.2);
        scalar("historyDecay", 0.95);
        scalar("historyStep", 1.0);
        glViewport(0, 0, width, height);
        QCOMPARE(glGetError(), GLenum(GL_NO_ERROR));
    }
    void disabledEffectsPreserveDesktop()
    {
        setMusic(true);
        const auto result = render({});
        QVERIFY(difference(input, result) < 0.1);
    }
    void everyEffectProducesVisibleOutput()
    {
        setMusic(true);
        std::array<int, PulsePointer::screenEffectCount> strengths{};
        const auto base = render(strengths);
        QImage sheet(width * 4, (height + 28) * 5, QImage::Format_RGB32);
        sheet.fill(Qt::black);
        QPainter painter(&sheet);
        painter.setPen(Qt::white);
        for (std::size_t i = 0; i < strengths.size(); ++i) {
            strengths.fill(0);
            strengths[i] = 100;
            const auto result = render(strengths);
            const auto &info = PulsePointer::screenEffectCatalog[i];
            QVERIFY2(difference(base, result) > 0.02, info.name);
            QCOMPARE(glGetError(), GLenum(GL_NO_ERROR));
            const QPoint position((i % 4) * width, (i / 4) * (height + 28));
            painter.drawImage(position, result.transformed(QTransform::fromScale(1, -1)));
            painter.drawText(position + QPoint(8, height + 19), QString::fromLatin1(info.name));
        }
        painter.end();
        const auto artifacts = qEnvironmentVariable("PULSEPOINTER_TEST_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QDir().mkpath(artifacts);
            QVERIFY(sheet.save(artifacts + QStringLiteral("/effects.png")));
        }
    }
    void borderProfilesHaveDistinctShapes()
    {
        setMusic(true);
        std::array<int, PulsePointer::screenEffectCount> strengths{};
        strengths[std::size_t(PulsePointer::ScreenEffect::RazorFrame)] = 100;
        glUniform1i(glGetUniformLocation(program, "profile"), 0);
        auto previous = render(strengths);
        for (int profile = 1; profile <= 4; ++profile) {
            glUniform1i(glGetUniformLocation(program, "profile"), profile);
            const auto result = render(strengths);
            QVERIFY(difference(previous, result) > 0.02);
            previous = result;
        }
        glUniform1i(glGetUniformLocation(program, "profile"), 0);
    }
    void refractiveRipplesFollowPointer()
    {
        setMusic(true);
        vec4("waveAge", 0.18, 10, 10, 10);
        vec4("waveStrength", 1, 0, 0, 0);
        const auto base = render({});
        std::array<int, PulsePointer::screenEffectCount> strengths{};
        strengths[std::size_t(PulsePointer::ScreenEffect::Refraction)] = 100;
        QImage previous;
        for (const QPointF pointer : {QPointF(0.2, 0.25), QPointF(0.8, 0.75)}) {
            glUniform2f(glGetUniformLocation(program, "pointer"), pointer.x(), pointer.y());
            const auto result = render(strengths);
            // Readback has its origin at the bottom, while pointer coordinates
            // start at the top. Test both axes away from the screen center.
            const QPoint center(qRound(pointer.x() * width), qRound((1 - pointer.y()) * height));
            const QRect nearPointer(center - QPoint(32, 32), QSize(64, 64));
            const QRect screenCenter(QPoint(width / 2 - 32, height / 2 - 32), QSize(64, 64));
            const double focused = difference(base.copy(nearPointer), result.copy(nearPointer));
            const double central = difference(base.copy(screenCenter), result.copy(screenCenter));
            QVERIFY(focused > 0.1);
            QVERIFY2(focused > central * 3, "Ripples must focus on the pointer, not the screen center");
            if (!previous.isNull()) QVERIFY(difference(previous, result) > 0.02);
            previous = result;
        }
        glUniform2f(glGetUniformLocation(program, "pointer"), 0.4, 0.55);
        QCOMPARE(glGetError(), GLenum(GL_NO_ERROR));
    }
    void combinedEffectsSettleToNormalInSilence()
    {
        std::array<int, PulsePointer::screenEffectCount> strengths;
        strengths.fill(100);
        setMusic(true);
        QVERIFY(difference(input, render(strengths)) > 5);
        setMusic(false);
        QVERIFY(difference(input, render(strengths)) < 0.1);
        QCOMPARE(glGetError(), GLenum(GL_NO_ERROR));
    }
    void cleanupTestCase()
    {
        if (context != EGL_NO_CONTEXT) {
            glDeleteProgram(program);
            glDeleteFramebuffers(1, &framebuffer);
            const GLuint textures[]{target, scene, history, audio};
            glDeleteTextures(4, textures);
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &vao);
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, context);
        }
        if (display != EGL_NO_DISPLAY) eglTerminate(display);
    }
};
QTEST_MAIN(ScreenShaderTest)
#include "screen_shader_test.moc"
