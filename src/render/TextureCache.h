// raylib uploads a separate GPU texture for every material of every glTF it loads. Our town chunks
// each embed their own copy of the same ~30 textures, so we hash the pixels, keep one copy of each,
// and give them mipmaps + anisotropic filtering (the ground is seen at a grazing angle).
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "raylib.h"

namespace journey {

class TextureCache {
public:
    // Replaces the model's diffuse textures with shared, mipmapped copies.
    void Share(Model& model);
    // Detaches the shared textures from `model` so UnloadModel does not free them twice.
    void Detach(Model& model) const;
    void UnloadAll();
    int UniqueCount() const { return (int)owned_.size(); }
    int DuplicatesDropped() const { return dropped_; }

private:
    std::unordered_map<uint64_t, Texture2D> byHash_;
    std::vector<Texture2D> owned_;
    int dropped_ = 0;
};

}  // namespace journey
