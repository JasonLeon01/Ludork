#include "ShaderCacheImpl.hpp"

#include <Runtime/Graphics/ShaderCompiler.hpp>
#include <Runtime/Json.hpp>
#include <BackgroundTask.hpp>
#include <BatchWorkerPool.hpp>
#include <FileWorkers.hpp>
#include <Sha256.hpp>

#include <algorithm>
#include <stdexcept>

namespace ludork::runtime::graphics {

std::string ShaderCache::Impl::compilationIdentity(
    const std::string& compiler, ShaderTranslator::Stage stage,
    const std::string& entry, const std::string& contentHash) {
    return ludork::standard::sha256(stringifyJSON(RuntimeData(
        RuntimeData::Array{RuntimeData(compiler),
                           RuntimeData(static_cast<std::int64_t>(stage)),
                           RuntimeData(entry), RuntimeData(contentHash)})));
}

double ShaderCache::Impl::compilePrograms(
    std::vector<ProgramPreparation>& plans,
    std::deque<CachedFile>& compiledBinaries,
    ShaderTranslator::Language language, const std::string& compiler,
    const std::string& profile, ludork::standard::BatchWorkerPool& pool,
    const std::atomic<bool>& stop,
    const std::function<void(double)>& progress) {
    double completedProgress = 0.1;
    const auto runBatch = [&](std::size_t count, const auto& work) {
        if (count == 0) {
            return;
        }
        const double start = completedProgress;
        completedProgress += (0.9 - start) * static_cast<double>(count) /
                             static_cast<double>(count + plans.size());
        const auto width = 2 * ludork::standard::fileWorkerCount();
        for (std::size_t first = 0; first < count;) {
            const auto size = std::min(count - first, width);
            pool.run(size, [&](std::size_t index) {
                checkCancelled(stop);
                work(first + index);
            });
            first += size;
            checkCancelled(stop);
            progress(start + (completedProgress - start) *
                                 static_cast<double>(first) /
                                 static_cast<double>(count));
            ludork::standard::BackgroundTask::yield();
        }
    };

    std::vector<ProgramPreparation*> translations;
    for (auto& plan : plans) {
        if (plan.needsTranslation) {
            translations.push_back(&plan);
        }
    }
    runBatch(translations.size(), [&](std::size_t index) {
        auto& plan = *translations[index];
        auto outputs =
            ShaderTranslator::translate(plan.program.source, language);
        for (auto& stage : plan.stages) {
            const auto found = std::find_if(
                outputs.begin(), outputs.end(), [&](const auto& output) {
                    return output.stage == stage.source->stage;
                });
            if (found == outputs.end()) {
                throw std::logic_error("Shader translation omitted a stage: " +
                                       stage.source->name);
            }
            stage.output = std::move(*found);
            stage.contentHash = ludork::standard::sha256(stage.output.text);
            if (language == ShaderTranslator::Language::Hlsl) {
                auto identity = compilationIdentity(
                    compiler, stage.source->stage, stage.output.entryPoint,
                    stage.contentHash);
                if (identity != stage.compilation) {
                    stage.binaryFile = nullptr;
                }
                stage.compilation = std::move(identity);
            }
        }
    });
    if (language != ShaderTranslator::Language::Hlsl) {
        return completedProgress;
    }

    std::map<std::string, const CachedFile*> binaries;
    for (const auto& plan : plans) {
        for (const auto& stage : plan.stages) {
            if (stage.binaryFile) {
                binaries.try_emplace(stage.compilation, stage.binaryFile);
            }
        }
    }
    std::vector<StagePreparation*> missing;
    for (auto& plan : plans) {
        checkCancelled(stop);
        for (auto& stage : plan.stages) {
            if (stage.binaryFile) {
                continue;
            }
            auto [position, inserted] = binaries.try_emplace(stage.compilation);
            if (inserted) {
                position->second = &compiledBinaries.emplace_back();
                missing.push_back(&stage);
            }
            stage.binaryFile = position->second;
        }
    }
    runBatch(missing.size(), [&](std::size_t index) {
        auto& stage = *missing[index];
        const auto bytes = ShaderCompiler::compileDxbc(
            stage.output.text, stage.source->stage, stage.output.entryPoint,
            stage.target, profile);
        auto& file = compiledBinaries[index];
        file.bytes.assign(reinterpret_cast<const char*>(bytes.data()),
                          bytes.size());
        if (!file.bytes.starts_with("DXBC")) {
            throw std::runtime_error(
                "Shader compiler did not produce raw DXBC: " + stage.target +
                ".dxbc");
        }
        file.hash = ludork::standard::sha256(file.bytes);
        file.exists = true;
    });
    return completedProgress;
}

}  // namespace ludork::runtime::graphics
