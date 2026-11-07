#include "screenrenderer.h"

#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/eglcontext.h"
#include "opengl/glframebuffer.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/gltexture.h"

#include <QFile>
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <utility>

namespace KWin
{
namespace
{
// GLTexture allocation and framebuffer blits may bind textures too, so save
// texture units before any allocation, including on early-failure paths.
struct ScreenGlStateGuard {
    GLint active;
    std::array<GLint, 3> bindings;
    bool blend;
    bool scissor;
    ScreenGlStateGuard()
    {
        blend = glIsEnabled(GL_BLEND);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &bindings[i]);
        }
        glActiveTexture(GL_TEXTURE0);
    }
    ~ScreenGlStateGuard()
    {
        if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, bindings[i]);
        }
        glActiveTexture(active);
    }
};
struct Capture {
    // Declaration order ensures framebuffers are destroyed before their attachments.
    std::unique_ptr<GLTexture> texture;
    std::unique_ptr<GLFramebuffer> framebuffer;
    bool allocate(const QSize &size)
    {
        framebuffer.reset();
        texture = GLTexture::allocate(GL_RGBA16F, size);
        if (!texture) {
            return false;
        }
        texture->setFilter(GL_LINEAR);
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        framebuffer = std::make_unique<GLFramebuffer>(texture.get());
        return framebuffer->valid();
    }
};
QByteArray shaderSource(const char *path)
{
    QFile file(QString::fromLatin1(path));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QVector4D vector(const std::array<float, 4> &values)
{
    return {values[0], values[1], values[2], values[3]};
}
template<typename Target>
const ColorDescription &colorDescription(const Target &target)
{
    if constexpr (requires { target.colorDescription().get(); }) {
        return *target.colorDescription();
    } else {
        return target.colorDescription();
    }
}
template<typename Viewport>
OutputTransform viewportTransform(const Viewport &viewport, const RenderTarget &target)
{
    if constexpr (requires { viewport.transform(); }) {
        return viewport.transform();
    } else {
        return target.transform();
    }
}
} // namespace

struct ScreenRenderer::Private {
    struct OutputState {
        Capture scene;
        Capture history;
        decltype(std::declval<RenderViewport>().renderRect()) geometry;
        OutputTransform transform;
        std::optional<ColorDescription> colors;
        bool historyValid = false;
        QElapsedTimer frameClock;
    };
    std::unique_ptr<GLShader> shader;
    std::unique_ptr<GLTexture> audio;
    std::map<ScreenOutput, OutputState> outputs;
    bool failed = false;
    bool initialized = false;

    bool initialize()
    {
        if (initialized) {
            return !failed;
        }
        initialized = true;
        auto *context = EglContext::currentContext();
        if (!context || !context->supportsBlits()) {
            failed = true;
            qWarning("PulsePointer: screen effects require OpenGL framebuffer support");
            return false;
        }
        shader = ShaderManager::instance()->loadShaderFromCode(
            shaderSource(":/pulsepointer/shaders/screen.vert"),
            shaderSource(":/pulsepointer/shaders/screen.frag"));
        audio = GLTexture::allocate(GL_R32F, QSize(160, 1));
        failed = !shader || !audio;
        if (failed) {
            qWarning("PulsePointer: could not initialize screen effects; pointer effects remain available");
        } else {
            audio->setFilter(GL_LINEAR);
            audio->setWrapMode(GL_CLAMP_TO_EDGE);
        }
        return !failed;
    }
};

ScreenRenderer::ScreenRenderer()
    : d(std::make_unique<Private>())
{
}
ScreenRenderer::~ScreenRenderer() = default;

bool ScreenRenderer::available() const
{
    return !d->failed && effects->isOpenGLCompositing();
}

void ScreenRenderer::invalidateHistory()
{
    for (auto &[screen, output] : d->outputs) {
        Q_UNUSED(screen);
        output.historyValid = false;
        output.frameClock.invalidate();
    }
}

bool ScreenRenderer::render(const RenderTarget &target, const RenderViewport &viewport,
                            ScreenOutput screen, const PulsePointer::Settings &settings,
                            const PulsePointer::MusicMotion &motion,
                            const std::array<float, 32> &spectrum,
                            const std::array<float, 128> &waveform, const QPointF &cursor)
{
    if (!target.framebuffer()) {
        return false;
    }
    const ScreenGlStateGuard glState;
    if (!d->initialize()) {
        return false;
    }
    const auto screens = effects->screens();
    std::erase_if(d->outputs, [&screens](const auto &entry) { return !screens.contains(entry.first); });
    auto &output = d->outputs[screen];
    const auto geometry = viewport.renderRect();
    const QSize captureSize = (geometry.size() * viewport.scale()).toSize();
    if (captureSize.isEmpty()) {
        return false;
    }
    if (!output.scene.texture || output.scene.texture->size() != captureSize) {
        if (!output.scene.allocate(captureSize)) {
            d->failed = true;
            qWarning("PulsePointer: could not allocate a screen capture");
            return false;
        }
        output.historyValid = false;
    }
    const auto transform = viewportTransform(viewport, target);
    const auto &colors = colorDescription(target);
    if (output.geometry != geometry || output.transform != transform
        || !output.colors || *output.colors != colors) {
        output.historyValid = false;
    }
    output.geometry = geometry;
    output.transform = transform;
    output.colors = colors;
    const bool needsHistory = settings.strength(PulsePointer::ScreenEffect::Feedback) > 0
        || settings.strength(PulsePointer::ScreenEffect::ColorEchoes) > 0;
    if (needsHistory && (!output.history.texture || output.history.texture->size() != captureSize)) {
        if (!output.history.allocate(captureSize)) {
            d->failed = true;
            qWarning("PulsePointer: could not allocate visual echo history");
            return false;
        }
        output.historyValid = false;
    } else if (!needsHistory) {
        output.history.framebuffer.reset();
        output.history.texture.reset();
        output.historyValid = false;
    }
    const auto captureRect = decltype(geometry.toRect())(QPoint(), captureSize);
    if (!output.scene.framebuffer->blitFromRenderTarget(target, viewport, geometry.toRect(), captureRect)) {
        output.historyValid = false;
        return false;
    }

    // GL calls below run only during a compositor paint, when its context is current.
    glActiveTexture(GL_TEXTURE2);
    d->audio->bind();
    std::array<float, 160> audio;
    std::copy(spectrum.begin(), spectrum.end(), audio.begin());
    std::copy(waveform.begin(), waveform.end(), audio.begin() + spectrum.size());
    GLint unpackBuffer, rowLength, skipPixels, skipRows;
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 160, 1, GL_RED, GL_FLOAT, audio.data());
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
    glActiveTexture(GL_TEXTURE1);
    (output.historyValid ? output.history.texture : output.scene.texture)->bind();
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    {
        ShaderBinder binder(d->shader.get());
        auto *shader = binder.shader();
        QMatrix4x4 mvp = viewport.projectionMatrix();
        mvp.translate(geometry.x() * viewport.scale(), geometry.y() * viewport.scale());
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
        shader->setUniform("scene", 0);
        shader->setUniform("history", 1);
        shader->setUniform("audioData", 2);
        shader->setUniform("size", QVector2D(geometry.width(), geometry.height()));
        shader->setUniform("pointer", QVector2D((cursor.x() - geometry.x()) / geometry.width(),
                                               (cursor.y() - geometry.y()) / geometry.height()));
        shader->setUniform("bands", vector(motion.bands));
        shader->setUniform("waveAge", vector(motion.waveAge));
        shader->setUniform("waveStrength", vector(motion.waveStrength));
        for (std::size_t i = 0; i < PulsePointer::screenEffectCount / 4; ++i) {
            QVector4D strength;
            for (int j = 0; j < 4; ++j) {
                strength[j] = settings.screenEffects[i * 4 + j] * settings.screenIntensity / 10000.0f;
            }
            const QByteArray name = QByteArrayLiteral("fx") + QByteArray::number(i);
            shader->setUniform(name.constData(), strength);
        }
        const QColor color = QColor::fromRgba(settings.color);
        shader->setUniform("tint", QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
        shader->setUniform("time", motion.time);
        shader->setUniform("hue", motion.hue);
        shader->setUniform("pulse", motion.pulse);
        shader->setUniform("activity", motion.activity());
        shader->setUniform("reach", float(settings.edgeReach));
        shader->setUniform("density", float(settings.edgeDensity));
        shader->setUniform("softness", settings.edgeSoftness / 100.0f);
        shader->setUniform("profile", settings.edgeProfile);
        shader->setUniform("collapse", motion.collapse());
        shader->setUniform("dropAge", motion.dropAge);
        shader->setUniform("dropStrength", motion.dropStrength);
        shader->setUniform("historyValid", output.historyValid ? 1.0f : 0.0f);
        const float frameSeconds = output.frameClock.isValid()
            ? std::clamp(output.frameClock.restart() / 1000.0f, 0.001f, 0.1f) : 1.0f / 60.0f;
        output.frameClock.start();
        shader->setUniform("historyDecay", std::exp(-frameSeconds / (settings.releaseMs / 1000.0f)));
        shader->setUniform("historyStep", frameSeconds * 60.0f);
        output.scene.texture->render(geometry.size() * viewport.scale());
    }

    if (needsHistory) {
        output.historyValid = output.history.framebuffer->blitFromRenderTarget(
            target, viewport, geometry.toRect(), captureRect);
    }
    return true;
}
} // namespace KWin
