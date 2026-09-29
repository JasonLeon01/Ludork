#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeData.hpp>

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ludork::video {

class LUDORK_ENGINE_API SubtitleTrack {
public:
    struct Section {
        double startTime = 0;
        double endTime = 0;
        std::variant<std::vector<std::string>,
                     std::unordered_map<std::string, std::vector<std::string>>>
            content;
    };

    static SubtitleTrack parse(const RuntimeData& asset);
    static void validatePath(const std::string& dataPath);
    static SubtitleTrack load(const std::string& dataPath);
    const std::vector<std::string>* linesAt(double time,
                                            const std::string& language) const;
    double duration() const noexcept;

private:
    std::vector<Section> sections_;
};

}  // namespace ludork::video
