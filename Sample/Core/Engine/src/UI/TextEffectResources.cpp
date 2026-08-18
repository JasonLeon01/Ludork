#include <UI/TextEffectResources.hpp>

#include <ConcurrentResourceCache.hpp>
#include <Utils/ShaderLoader.hpp>

#include <SFML/Graphics/Shader.hpp>

#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <unordered_set>

namespace ludork::engine::text_effects {

namespace {

constexpr const char* DefaultTextEffectsShader = "Global/TextEffects.frag";

class TextShaderLoadError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct TextEffectResourceState {
    ludork::core::ConcurrentResourceCache<sf::Shader> shaders;
    std::mutex mutex;
    std::unordered_set<std::string> failedShaders;
    std::unordered_set<std::string> warnings;
    std::uint64_t generation = 0;
};

TextEffectResourceState& resourceState() {
    static TextEffectResourceState state;
    return state;
}

}  // namespace

void warnOnce(const std::string& key, const std::string& message) {
    TextEffectResourceState& state = resourceState();
    bool inserted;
    {
        const std::lock_guard<std::mutex> lock(state.mutex);
        inserted = state.warnings.insert(key).second;
    }
    if (inserted) {
        std::cerr << message << '\n';
    }
}

std::shared_ptr<sf::Shader> loadShader() {
    TextEffectResourceState& state = resourceState();
    const std::string path = DefaultTextEffectsShader;
    if (!sf::Shader::isAvailable()) {
        warnOnce("Text.shaderUnavailable",
                 "Text effects are disabled because shaders are unavailable");
        return nullptr;
    }
    std::uint64_t generation;
    {
        const std::lock_guard<std::mutex> lock(state.mutex);
        if (state.failedShaders.contains(path)) {
            return nullptr;
        }
        generation = state.generation;
    }
    try {
        return state.shaders.getOrLoad(path, [&]() {
            ShaderLoadResult loaded =
                ShaderLoader::load(path, sf::Shader::Type::Fragment);
            if (!loaded) {
                throw TextShaderLoadError(loaded.error);
            }
            return loaded.shader;
        });
    } catch (const TextShaderLoadError& error) {
        bool activeGeneration;
        {
            const std::lock_guard<std::mutex> lock(state.mutex);
            activeGeneration = state.generation == generation;
            if (activeGeneration) {
                state.failedShaders.insert(path);
            }
        }
        if (activeGeneration) {
            warnOnce("Text.shaderLoad:" + path, error.what());
        }
        return nullptr;
    }
}

void clearResources() noexcept {
    TextEffectResourceState& state = resourceState();
    const std::lock_guard<std::mutex> lock(state.mutex);
    ++state.generation;
    state.shaders.clear();
    state.failedShaders.clear();
    state.warnings.clear();
}

}  // namespace ludork::engine::text_effects
