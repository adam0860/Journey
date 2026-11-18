// The one shader every 3D object uses: gamma-correct sun + sky/ground ambient + distance fog,
// plus (style B) texture sampling, a sun shadow map, filmic grading and animated water.
// The same source builds for desktop GL 3.3 and Android GLES2 (a small prefix switches dialects).
#pragma once
#include "raylib.h"

namespace journey {

// Convert a colour you picked on screen (sRGB) into the linear value the shader expects.
Color ToLinear(Color srgb);

class LitShader {
public:
    bool Load();
    void Unload();
    void ApplyTo(Model& model) const;

    // Per-frame state. fidelity=true turns on textures-grading/water; shadows additionally needs BeginShadowPass.
    void SetFrame(Vector3 cameraPos, float timeSeconds, bool fidelity);

    // Sun shadow map (style B). Call Begin/End around the depth draws (before BeginMode3D of the real
    // camera), then BindShadow() once inside the real BeginMode3D, before drawing lit things.
    void BeginShadowPass(float focusX, float focusZ);
    void EndShadowPass();
    void BindShadow(bool enabled);
    bool ShadowReady() const { return shadowReady_; }
    // sample=false while the shadow map is being drawn (avoids reading the texture we render into).
    void SetShadowSampling(Model& model, bool sample) const;

    // Clear colour (sRGB) that matches the fog, so distant terrain melts into the sky.
    static Color SkyColor();

private:
    Shader shader_{};
    RenderTexture2D shadowRT_{};
    Matrix lightVP_{};
    bool shadowReady_ = false;
    int locViewPos_ = -1, locDepthPass_ = -1, locShadowOn_ = -1, locGrade_ = -1, locTime_ = -1;
    int locLightVP_ = -1, locTexel_ = -1;
};

}  // namespace journey
