#pragma once

#include <Runtime/Graphics/ShaderCache.hpp>
#include <Runtime/RuntimeData.hpp>

#include <map>
#include <mutex>
#include <optional>

namespace ludork::runtime::graphics {

struct ShaderCache::Impl {
    struct StageFile {
        ShaderTranslator::Stage stage;
        std::string path;
        std::string hash;
        bool encrypted = false;
    };
    struct StoredProgram {
        std::vector<StageFile> stages;
        std::map<ShaderTranslator::Stage, std::string> bytes;
        sf::rhi::ShaderOptions options;
        std::shared_ptr<sf::Shader> shader;
    };
    struct Output {
        std::filesystem::path path;
        std::string bytes;
        std::optional<RuntimeData> record;
    };
    struct CachedFile {
        bool exists = false;
        std::string bytes;
        std::string hash;
    };
    struct StagePreparation {
        const ShaderTranslator::Source* source = nullptr;
        const RuntimeData* record = nullptr;
        std::string sourceHash;
        std::string target;
        bool encrypted = false;
        std::string savedTarget;
        const CachedFile* targetFile = nullptr;
        const CachedFile* binaryFile = nullptr;
        std::optional<std::string> text;
    };
    struct ProgramPreparation {
        Program program;
        std::vector<StagePreparation> stages;
        std::string key;
        std::string translation;
        bool needsTranslation = false;
    };
    struct PrewarmTarget {
        std::shared_ptr<sf::rhi::Texture> texture;
        std::shared_ptr<sf::rhi::RenderTarget> target;
    };

    std::mutex mutex;
    std::mutex preparationMutex;
    std::map<std::string, StoredProgram> programs;
    std::filesystem::path root;
    bool encryptShaders = false;
    bool encryptData = false;
    sf::ContextSettings windowSettings;
    std::vector<sf::BlendMode> blendModes;

    static RuntimeData describe(const Program& program, bool includeText);
    static std::shared_ptr<sf::Shader> create(
        const Program& program,
        const std::map<ShaderTranslator::Stage, std::string>& sources);
    static std::filesystem::path physical(const std::filesystem::path& root,
                                          const std::string& logical);
    static std::string encode(const std::string& bytes,
                              const std::string& identity, bool encrypted);
    static std::string decode(const std::string& bytes,
                              const std::string& identity, bool encrypted);
    static void publish(const std::filesystem::path& root,
                        const std::vector<Output>& outputs,
                        const std::set<std::filesystem::path>& obsolete,
                        const std::atomic<bool>& stop);
    static RuntimeData readRecord(const std::filesystem::path& path);
    static bool validFile(const std::filesystem::path& path,
                          const std::string& hash);
    static void prewarm(const std::shared_ptr<sf::rhi::Device>& device,
                        const std::shared_ptr<sf::Shader>& shader,
                        const sf::ContextSettings& windowSettings,
                        const std::vector<sf::BlendMode>& extraBlendModes,
                        std::vector<PrewarmTarget>& targets);
    void prepare(const std::filesystem::path& userDataRoot,
                 const std::vector<Program>& inputs, bool encryptSources,
                 bool encryptMetadata, const std::atomic<bool>& stop,
                 const std::function<void(double)>& progress,
                 const MaterialProjection* projectedMaterials,
                 const std::function<void(bool)>& onPlannedWithLocks);
    void savePipelineCache();
};

}  // namespace ludork::runtime::graphics
