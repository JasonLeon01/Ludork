#pragma once
#include <Runtime/Graphics/GpuEmitterBackend.hpp>
#include <Runtime/Graphics/GpuEmitterConfiguration.hpp>
#include "GpuTrackImpl.hpp"
#include "GpuStatisticsImpl.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ludork::runtime::graphics {

struct GpuEmitterBackend::Impl {
    struct Query {
        unsigned int id = 0;
        bool pending = false;
        bool simulation = false;
        std::int64_t sample = 0;
        std::uint64_t generation = 0;
        double time = 0;
    };
    GpuEmitterConfiguration data;
    std::shared_ptr<GpuResourcesImpl> resources;
    std::vector<std::unique_ptr<GpuTrackImpl>> tracks;
    std::unique_ptr<GpuStatisticsImpl> statistics;
    std::optional<GpuStatisticsImpl::Sample> sample;
    sf::Transform host;
    sf::Vector2f previousPosition;
    sf::Vector2f accumulatedMotion;
    bool hasPosition = false;
    bool playing = false;
    bool draining = false;
    bool reset = true;
    bool profiling = false;
    bool finished = false;
    std::unordered_map<std::string, int> pending;
    float speed = 1;
    sf::Color colour = sf::Color::White;
    double time = 0;
    double remainder = 0;
    std::array<Query, 16> queries{};
    std::size_t queryCursor = 0;
    double simulationMs = -1;
    double drawMs = -1;
    std::int64_t frame = 0;
    std::uint64_t generation = 0;
    std::int64_t simulationSample = 0;
    std::int64_t drawSample = 0;
    void prepare();
    Query* beginQuery(bool simulation);
    void endQuery(Query* query);
    void pollQueries();
    void sampleStatistics();
};

}  // namespace ludork::runtime::graphics
