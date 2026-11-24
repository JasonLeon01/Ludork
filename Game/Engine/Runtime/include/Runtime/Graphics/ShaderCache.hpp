#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/Graphics/ShaderTranslator.hpp>
#include <SFML/Graphics/RHI/Device.hpp>
#include <SFML/Graphics/Shader.hpp>

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace ludork::runtime::graphics {

class LUDORK_RUNTIME_API ShaderCache {
public:
    struct Program {
        ShaderTranslator::Program source;
        sf::rhi::ShaderOptions options;
        std::set<ShaderTranslator::Stage> encryptedStages;
        std::map<std::string, std::string> inputHashes;
        std::vector<sf::BlendMode> blendModes;
        bool generated = false;
    };

    struct MaterialProjection {
        static constexpr std::string_view Prefix =
            "/Game/Assets/Shaders/MaterialGenerated/";
        struct Artifact {
            std::string bytes;
            std::string hash;
            std::filesystem::file_time_type modified;
        };
        std::map<std::string, std::string> sources;
        std::map<std::string, Artifact> artifacts;
    };

    static std::string key(const Program& program);
    static std::shared_ptr<sf::Shader> load(const Program& program);
    static std::shared_ptr<sf::Shader> instantiate(const Program& program);
    static std::shared_ptr<sf::Shader> load(const std::string& key);
    static void prepare(
        const std::filesystem::path& userDataRoot,
        const std::vector<Program>& programs, bool encryptShaders,
        bool encryptData, const std::atomic<bool>& stop,
        const std::function<void(double)>& progress,
        const MaterialProjection* projectedMaterials = nullptr,
        const std::function<void(bool)>& onPlannedWithLocks = {});
    static void withPreparationLock(const std::atomic<bool>& stop,
                                    const std::function<void()>& work);
    static void loadPipelineCache(const std::filesystem::path& userDataRoot);
    static void savePipelineCache();
    static void setWindowSettings(const sf::ContextSettings& settings,
                                  std::vector<sf::BlendMode> blendModes = {});
    static void clear();
    static bool isDerivedFile(const std::string& path,
                              const std::set<std::string>& sources);

private:
    struct Impl;
    static Impl& impl();
};

}  // namespace ludork::runtime::graphics
