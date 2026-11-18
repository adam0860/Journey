#include "render/LitShader.h"

#include <cmath>
#include <string>

#include "raymath.h"
#include "rlgl.h"

namespace journey {

namespace {

constexpr int kShadowSize = 2048;       // shadow map resolution
constexpr float kShadowSpan = 58.0f;    // world units covered (square) around the player

#if defined(__ANDROID__) || defined(GRAPHICS_API_OPENGL_ES2)   // phones (and the desktop ES2 test build)
const char* kVertexPrefix = "#version 100\n#define VIN attribute\n#define VOUT varying\n";
const char* kFragmentPrefix =
    "#version 100\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
    "#define FIN varying\n#define finalColor gl_FragColor\n";
#else
const char* kVertexPrefix = "#version 330\n#define VIN in\n#define VOUT out\n";
const char* kFragmentPrefix = "#version 330\n#define FIN in\nout vec4 finalColor;\n#define texture2D texture\n";
#endif

const char* kVertexBody = R"GLSL(
VIN vec3 vertexPosition;
VIN vec2 vertexTexCoord;
VIN vec3 vertexNormal;
VIN vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matLightVP;
VOUT vec3 fragPos;
VOUT vec3 fragNormal;
VOUT vec4 fragColor;
VOUT vec2 fragUV;
VOUT vec4 fragLight;
void main() {
    fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = vec3(matModel * vec4(vertexNormal, 0.0));
    fragColor = vertexColor;
    fragUV = vertexTexCoord;
    fragLight = matLightVP * vec4(fragPos, 1.0);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

const char* kFragmentBody = R"GLSL(
FIN vec3 fragPos;
FIN vec3 fragNormal;
FIN vec4 fragColor;
FIN vec2 fragUV;
FIN vec4 fragLight;
uniform sampler2D texture0;
uniform sampler2D texture1;   // sun shadow map; raylib binds it per draw via material map slot 1
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform float uDepthPass;   // 1 while rendering the shadow map
uniform float uShadowOn;    // 1 when the shadow map is valid
uniform float uGrade;       // 1 = style B look (filmic grade, animated water)
uniform float uTime;
uniform vec2 uTexel;        // 1/shadow map size

// Lighting rig (all linear colour). Sun comes from the south-west so the faces the camera sees are lit.
const vec3 sunDir       = vec3(-0.46, 0.74, 0.49);
const vec3 sunColor     = vec3(1.05, 0.93, 0.78);
const vec3 skyAmbient   = vec3(0.30, 0.38, 0.52);
const vec3 groundAmbient= vec3(0.22, 0.19, 0.13);
const vec3 fogColor     = vec3(0.40, 0.62, 0.82);
const vec2 fogRange     = vec2(32.0, 72.0);

vec3 packDepth(float d) {
    vec3 enc = fract(vec3(1.0, 255.0, 65025.0) * d);
    enc -= enc.yzz * vec3(1.0 / 255.0, 1.0 / 255.0, 0.0);
    return enc;
}
float unpackDepth(vec3 c) {
    return dot(c, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0));
}

// 0 = fully shadowed, 1 = fully lit. 3x3 PCF on packed depth.
// Every sample is taken unconditionally (no texture reads inside non-uniform branches).
float sunVisibility(float ndl) {
    vec3 ndc = fragLight.xyz / fragLight.w;
    vec2 uvRaw = ndc.xy * 0.5 + 0.5;
    float depth = ndc.z * 0.5 + 0.5;
    vec2 uv = clamp(uvRaw, 0.01, 0.99);
    float bias = 0.0010 + 0.0016 * (1.0 - ndl);
    float lit = 0.0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            vec2 o = vec2(float(i), float(j)) * uTexel * 1.25;
            float sd = unpackDepth(texture2D(texture1, uv + o).rgb);
            lit += (depth - bias <= sd) ? 1.0 : 0.0;
        }
    }
    lit /= 9.0;
    bool outside = (uvRaw.x < 0.01 || uvRaw.x > 0.99 || uvRaw.y < 0.01 || uvRaw.y > 0.99 || depth > 1.0);
    return outside ? 1.0 : lit;
}

void main() {
    if (uDepthPass > 0.5) {
        finalColor = vec4(packDepth(gl_FragCoord.z), 1.0);
        return;
    }
    vec3 n = normalize(fragNormal);
    vec3 V = normalize(viewPos - fragPos);
    vec3 L = normalize(sunDir);
    float ndl = max(dot(n, L), 0.0);

    vec3 tex = pow(texture2D(texture0, fragUV).rgb, vec3(2.2));     // textures are authored in sRGB
    float spec = 0.0;
    if (uGrade > 0.5) {   // uniform branch: style B only
        // river surface: two scrolling samples + a sun glint (samples are unconditional, selection is not)
        vec2 a = fragUV + vec2(uTime * 0.020, uTime * 0.013);
        vec2 b = fragUV * 1.9 + vec2(-uTime * 0.017, uTime * 0.011);
        vec3 t1 = pow(texture2D(texture0, a).rgb, vec3(2.2));
        vec3 t2 = pow(texture2D(texture0, b).rgb, vec3(2.2));
        bool isWater = (n.y > 0.9 && fragPos.y < -0.343 && fragPos.y > -0.36);
        if (isWater) {
            tex = 0.5 * (t1 + t2);
            vec3 H = normalize(V + L);
            float glint = dot(t1 - t2, vec3(0.33)) * 6.0;
            spec = pow(max(H.y - glint * 0.04, 0.0), 220.0) * 1.1;
        }
    }
    vec3 albedo = colDiffuse.rgb * fragColor.rgb * tex;

    float vis = 1.0;
    if (uShadowOn > 0.5) {
        float v = sunVisibility(ndl);
        vis = (ndl > 0.0) ? v : 1.0;
    }
    vec3 ambient = mix(groundAmbient, skyAmbient, n.y * 0.5 + 0.5);
    vec3 col = albedo * (ambient + sunColor * ndl * vis) + sunColor * spec * vis;

    float d = length(viewPos - fragPos);
    col = mix(col, fogColor, clamp((d - fogRange.x) / (fogRange.y - fogRange.x), 0.0, 1.0));
    if (uGrade > 0.5) {
        col *= 1.05;                                            // exposure
        col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);   // ACES-style filmic curve
        float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
        col = mix(vec3(l), col, 1.0);
    }
    col = pow(clamp(col, 0.0, 1.0), vec3(1.0 / 2.2));
    finalColor = vec4(col, 1.0);
}
)GLSL";

}  // namespace

Color ToLinear(Color c) {
    auto f = [](unsigned char v) {
        return static_cast<unsigned char>(std::lround(std::pow(v / 255.0f, 2.2f) * 255.0f));
    };
    return {f(c.r), f(c.g), f(c.b), c.a};
}

Color LitShader::SkyColor() {
    return {170, 208, 238, 255};
}

bool LitShader::Load() {
    const std::string vs = std::string(kVertexPrefix) + kVertexBody;
    const std::string fs = std::string(kFragmentPrefix) + kFragmentBody;
    shader_ = LoadShaderFromMemory(vs.c_str(), fs.c_str());
    if (!IsShaderValid(shader_)) return false;
    locViewPos_ = GetShaderLocation(shader_, "viewPos");
    shader_.locs[SHADER_LOC_VECTOR_VIEW] = locViewPos_;
    locDepthPass_ = GetShaderLocation(shader_, "uDepthPass");
    locShadowOn_ = GetShaderLocation(shader_, "uShadowOn");
    locGrade_ = GetShaderLocation(shader_, "uGrade");
    locTime_ = GetShaderLocation(shader_, "uTime");
    locLightVP_ = GetShaderLocation(shader_, "matLightVP");
        locTexel_ = GetShaderLocation(shader_, "uTexel");

    const float texel[2] = {1.0f / kShadowSize, 1.0f / kShadowSize};
    SetShaderValue(shader_, locTexel_, texel, SHADER_UNIFORM_VEC2);
    return true;
}

void LitShader::Unload() {
    if (shadowRT_.id != 0) UnloadRenderTexture(shadowRT_);
    shadowRT_ = {};
    if (IsShaderValid(shader_)) UnloadShader(shader_);
    shader_ = {};
}

void LitShader::SetFrame(Vector3 cameraPos, float timeSeconds, bool fidelity) {
    SetShaderValue(shader_, locViewPos_, &cameraPos, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locTime_, &timeSeconds, SHADER_UNIFORM_FLOAT);
    const float g = fidelity ? 1.0f : 0.0f;
    SetShaderValue(shader_, locGrade_, &g, SHADER_UNIFORM_FLOAT);
}

void LitShader::ApplyTo(Model& model) const {
    for (int i = 0; i < model.materialCount; ++i) model.materials[i].shader = shader_;
}

void LitShader::BeginShadowPass(float focusX, float focusZ) {
    shadowReady_ = false;
    if (shadowRT_.id == 0) {   // created on first use: nothing is allocated while the experimental shadows are off
        shadowRT_ = LoadRenderTexture(kShadowSize, kShadowSize);
        if (shadowRT_.id == 0) return;
        SetTextureFilter(shadowRT_.texture, TEXTURE_FILTER_POINT);   // packed depth must not be interpolated
        SetTextureWrap(shadowRT_.texture, TEXTURE_WRAP_CLAMP);
    }

    // Light looks along -sunDir. Snap the box centre to whole shadow texels so edges do not crawl as we walk.
    const Vector3 L = Vector3Normalize({-0.46f, 0.74f, 0.49f});
    const Vector3 right = Vector3Normalize(Vector3CrossProduct({0, 1, 0}, L));
    const Vector3 up = Vector3CrossProduct(L, right);
    Vector3 focus = {focusX, 0.0f, focusZ};
    const float texelWorld = kShadowSpan / kShadowSize;
    const float cr = std::floor(Vector3DotProduct(focus, right) / texelWorld) * texelWorld;
    const float cu = std::floor(Vector3DotProduct(focus, up) / texelWorld) * texelWorld;
    focus = Vector3Add(Vector3Scale(right, cr), Vector3Scale(up, cu));
    // keep the (snapped) point on the ground plane along the light axis
    focus = Vector3Add(focus, Vector3Scale(L, -focus.y / L.y));

    Camera3D lightCam{};
    lightCam.position = Vector3Add(focus, Vector3Scale(L, 70.0f));
    lightCam.target = focus;
    lightCam.up = {0, 1, 0};
    lightCam.fovy = kShadowSpan;
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(shadowRT_);
    ClearBackground({255, 255, 255, 255});    // depth 1.0 = nothing in the way
    rlSetClipPlanes(5.0, 140.0);
    BeginMode3D(lightCam);
    lightVP_ = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    const float one = 1.0f;
    SetShaderValue(shader_, locDepthPass_, &one, SHADER_UNIFORM_FLOAT);
    shadowReady_ = true;
}

void LitShader::EndShadowPass() {
    if (!shadowReady_) return;
    EndMode3D();
    EndTextureMode();
    const float zero = 0.0f;
    SetShaderValue(shader_, locDepthPass_, &zero, SHADER_UNIFORM_FLOAT);
}

void LitShader::BindShadow(bool enabled) {
    const float on = (enabled && shadowReady_) ? 1.0f : 0.0f;
    SetShaderValue(shader_, locShadowOn_, &on, SHADER_UNIFORM_FLOAT);
    if (on > 0.5) SetShaderValueMatrix(shader_, locLightVP_, lightVP_);
}

// raylib binds a material's map #1 to texture unit 1 (and points sampler "texture1" at it) on EVERY draw, so the
// shadow map can never be left bound to a stale unit by other rlgl work. While the shadow map is being
// rendered, the slot holds a harmless 1x1 texture so we never sample the texture we are drawing into.
void LitShader::SetShadowSampling(Model& model, bool sample) const {
    Texture2D t = {rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if (sample && shadowRT_.id != 0) t = shadowRT_.texture;
    for (int i = 0; i < model.materialCount; ++i) model.materials[i].maps[MATERIAL_MAP_METALNESS].texture = t;
}

}  // namespace journey
