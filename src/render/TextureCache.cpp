#include "render/TextureCache.h"

#include "rlgl.h"

namespace journey {

namespace {
uint64_t HashPixels(const Image& img) {
    const unsigned char* p = static_cast<const unsigned char*>(img.data);
    const size_t n = (size_t)GetPixelDataSize(img.width, img.height, img.format);
    uint64_t h = 1469598103934665603ull ^ ((uint64_t)img.width << 20) ^ (uint64_t)img.height;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}
}  // namespace

void TextureCache::Share(Model& model) {
    for (int i = 0; i < model.materialCount; ++i) {
        Texture2D& tex = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
        if (tex.id == 0 || tex.id == rlGetTextureIdDefault()) continue;
        Image img = LoadImageFromTexture(tex);
        if (!img.data) continue;
        const uint64_t h = HashPixels(img);
        UnloadImage(img);
        auto it = byHash_.find(h);
        if (it != byHash_.end()) {
            UnloadTexture(tex);
            tex = it->second;
            ++dropped_;
            continue;
        }
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_ANISOTROPIC_8X);
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
        byHash_[h] = tex;
        owned_.push_back(tex);
    }
}

void TextureCache::Detach(Model& model) const {
    for (int i = 0; i < model.materialCount; ++i) {
        Texture2D& tex = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
        for (const Texture2D& o : owned_) {
            if (o.id == tex.id) {
                tex.id = rlGetTextureIdDefault();
                break;
            }
        }
    }
}

void TextureCache::UnloadAll() {
    for (Texture2D& t : owned_) UnloadTexture(t);
    owned_.clear();
    byHash_.clear();
    dropped_ = 0;
}

}  // namespace journey
