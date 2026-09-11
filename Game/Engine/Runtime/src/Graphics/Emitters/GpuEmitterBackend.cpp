#include <Runtime/Graphics/GpuEmitterBackend.hpp>
#include "GpuEmitterBackendImpl.hpp"
#include <SFML/Window/Context.hpp>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace ludork::runtime::graphics {
namespace {
std::mutex& retirementMutex() {
    static std::mutex mutex;
    return mutex;
}
std::vector<std::unique_ptr<GpuEmitterBackend>>& retiredEmitters() {
    static auto* entries = new std::vector<std::unique_ptr<GpuEmitterBackend>>;
    return *entries;
}
}  // namespace

GpuEmitterBackend::GpuEmitterBackend() : impl_(std::make_unique<Impl>()) {}
GpuEmitterBackend::~GpuEmitterBackend() {
    shutdown();
}
void GpuEmitterBackend::setConfiguration(
    GpuEmitterConfiguration configuration) {
    shutdown();
    impl_->data = std::move(configuration);
    restart();
}
void GpuEmitterBackend::play() {
    if (impl_->draining || !impl_->playing) {
        restart();
    }
}
void GpuEmitterBackend::pause() {
    impl_->playing = false;
    impl_->remainder = 0;
    impl_->accumulatedMotion = {};
}
void GpuEmitterBackend::resume() {
    impl_->playing = true;
    impl_->hasPosition = false;
}
void GpuEmitterBackend::restart() {
    ++impl_->generation;
    impl_->finished = false;
    impl_->pending.clear();
    impl_->sample.reset();
    impl_->simulationMs = -1;
    impl_->drawMs = -1;
    impl_->time = 0;
    impl_->remainder = 0;
    impl_->reset = true;
    impl_->playing = true;
    impl_->draining = false;
    impl_->hasPosition = false;
    impl_->accumulatedMotion = {};
}
void GpuEmitterBackend::stop(bool clear) {
    impl_->draining = true;
    impl_->pending.clear();
    if (clear) {
        ++impl_->generation;
        impl_->simulationMs = -1;
        impl_->drawMs = -1;
        impl_->finished = true;
        impl_->sample.reset();
        impl_->playing = false;
        impl_->reset = true;
        impl_->time = 0;
        impl_->remainder = 0;
    }
}
void GpuEmitterBackend::emit(const std::string& trackName, int count) {
    if (count < 0) {
        throw std::invalid_argument(
            "Particle emission count cannot be negative");
    }
    const auto found =
        std::find_if(impl_->data.tracks.begin(), impl_->data.tracks.end(),
                     [&](const auto& track) {
                         return track.name == trackName;
                     });
    if (found == impl_->data.tracks.end() || found->resident) {
        throw std::invalid_argument("Unknown emission track: " + trackName);
    }
    int& pending = impl_->pending[trackName];
    pending =
        static_cast<int>(std::min(static_cast<std::int64_t>(found->capacity),
                                  static_cast<std::int64_t>(pending) + count));
    impl_->finished = false;
    impl_->playing = true;
}
void GpuEmitterBackend::setSpeed(float speed) {
    if (!std::isfinite(speed) || speed < 0 || speed > 100) {
        throw std::invalid_argument("Emitter speed must be in [0,100]");
    }
    impl_->speed = speed;
}
float GpuEmitterBackend::getSpeed() const {
    return impl_->speed;
}
void GpuEmitterBackend::setColour(const sf::Color& colour) {
    impl_->colour = colour;
}
float GpuEmitterBackend::getTime() const {
    return static_cast<float>(impl_->time);
}
bool GpuEmitterBackend::isPlaying() const {
    return impl_->playing;
}
int GpuEmitterBackend::getCapacity() const {
    int capacity = 0;
    for (const auto& track : impl_->data.tracks) {
        if (track.enabled) {
            capacity += track.capacity;
        }
    }
    return capacity;
}
void GpuEmitterBackend::setProfiling(bool enabled) {
    impl_->profiling = enabled;
}
void GpuEmitterBackend::setHostTransform(const sf::Transform& transform) {
    impl_->host = transform;
}
void GpuEmitterBackend::resetHostMotion() {
    impl_->hasPosition = false;
    impl_->accumulatedMotion = {};
}
bool GpuEmitterBackend::isWarming() const {
    if (impl_->reset || impl_->tracks.empty()) {
        return std::any_of(impl_->data.tracks.begin(), impl_->data.tracks.end(),
                           [](const auto& track) {
                               return track.enabled && track.prewarm &&
                                      track.loop;
                           });
    }
    return std::any_of(impl_->tracks.begin(), impl_->tracks.end(),
                       [](const auto& track) {
                           return track->warmEnd > 0;
                       });
}
EmitterStatistics GpuEmitterBackend::getStatistics() const {
    EmitterStatistics result;
    result.capacity = getCapacity();
    result.time = impl_->time;
    if (impl_->resources) {
        result.renderer = impl_->resources->api.renderer;
    }
    if (impl_->sample) {
        result.aliveCount =
            static_cast<std::int64_t>(impl_->sample->aliveCount);
        result.sampledTime = impl_->sample->sampleTime;
    }
    if (impl_->simulationMs >= 0) {
        result.gpuSimulationMs = impl_->simulationMs;
        result.simulationSample = impl_->simulationSample;
    }
    if (impl_->drawMs >= 0) {
        result.gpuDrawMs = impl_->drawMs;
        result.drawSample = impl_->drawSample;
    }
    return result;
}
void GpuEmitterBackend::Impl::prepare() {
    if (!resources) {
        resources = ludork::runtime::graphics::GpuResourcesImpl::acquire();
    }
    if (tracks.empty()) {
        decltype(tracks) prepared;
        prepared.reserve(data.tracks.size());
        for (const auto& definition : data.tracks) {
            if (definition.enabled) {
                prepared.push_back(
                    std::make_unique<ludork::runtime::graphics::GpuTrackImpl>(
                        definition, resources));
            }
        }
        tracks.swap(prepared);
    }
    if (reset) {
        ludork::runtime::graphics::GpuStateGuard guard(resources->api);
        statistics.reset();
        sample.reset();
        for (auto& track : tracks) {
            track->reset();
            if (draining) {
                track->warmEnd = 0;
            }
        }
        reset = false;
    }
    for (auto& track : tracks) {
        const auto found = pending.find(track->definition.name);
        if (found != pending.end()) {
            track->pending = static_cast<int>(std::min(
                static_cast<std::int64_t>(track->definition.capacity),
                static_cast<std::int64_t>(track->pending) + found->second));
        }
    }
    pending.clear();
}
void GpuEmitterBackend::Impl::sampleStatistics() {
    if (!profiling) {
        return;
    }
    if (!statistics) {
        statistics =
            std::make_unique<ludork::runtime::graphics::GpuStatisticsImpl>(
                resources);
    }
    if (const auto value = statistics->poll()) {
        sample = value;
    }
    std::vector<std::pair<unsigned int, int>> buffers;
    buffers.reserve(tracks.size());
    for (const auto& track : tracks) {
        buffers.emplace_back(track->buffers[track->input],
                             track->definition.capacity);
    }
    statistics->submit(buffers, time,
                       finished && (!sample || sample->sampleTime < time));
}
GpuEmitterBackend::Impl::Query* GpuEmitterBackend::Impl::beginQuery(
    bool simulation) {
    pollQueries();
    if (!profiling || !resources->api.timerQueries) {
        return nullptr;
    }
    Query& query = queries[queryCursor];
    if (query.pending) {
        return nullptr;
    }
    if (!query.id) {
        resources->api.GenQueries(1, &query.id);
    }
    query.simulation = simulation;
    query.sample = frame;
    query.generation = generation;
    query.time = time;
    resources->api.BeginQuery(0x88BF, query.id);
    return &query;
}
void GpuEmitterBackend::Impl::endQuery(Query* query) {
    if (query == nullptr) {
        return;
    }
    resources->api.EndQuery(0x88BF);
    query->pending = true;
    queryCursor = (queryCursor + 1) % queries.size();
}
void GpuEmitterBackend::Impl::pollQueries() {
    if (!resources->api.timerQueries) {
        return;
    }
    int disjoint = 0;
    if (resources->api.embedded) {
        resources->api.GetIntegerv(0x8FBB, &disjoint);
    }
    for (Query& query : queries) {
        if (query.pending) {
            int available = 0;
            resources->api.GetQueryObjectiv(query.id, 0x8867, &available);
            if (available) {
                std::uint64_t nanoseconds = 0;
                resources->api.GetQueryObjectui64v(query.id, 0x8866,
                                                   &nanoseconds);
                if (!disjoint && query.generation == generation &&
                    query.sample >=
                        (query.simulation ? simulationSample : drawSample)) {
                    (query.simulation ? simulationMs : drawMs) =
                        static_cast<double>(nanoseconds) / 1000000;
                    (query.simulation ? simulationSample : drawSample) =
                        query.sample;
                }
                query.pending = false;
            }
        }
    }
    if (disjoint) {
        simulationMs = -1;
        drawMs = -1;
    }
}
void GpuEmitterBackend::tick(float deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime < 0) {
        throw std::invalid_argument("Invalid Emitter delta time");
    }
    collectGarbage();
    if (!impl_->playing || impl_->data.tracks.empty()) {
        return;
    }
    impl_->prepare();
    ludork::runtime::graphics::GpuStateGuard guard(impl_->resources->api);
    ++impl_->frame;
    Impl::Query* query = impl_->beginQuery(true);
    const double step = 1.0 / impl_->data.simulationRate;
    bool warming = false;
    for (int iteration = 0; iteration < 120; ++iteration) {
        warming = false;
        for (auto& track : impl_->tracks) {
            if (track->warmTime + 1e-6f < track->warmEnd) {
                const float previous = track->warmTime;
                track->warmTime = std::min(track->warmEnd,
                                           previous + static_cast<float>(step));
                track->step(previous, track->warmTime,
                            track->warmTime - previous, 0, {}, impl_->host,
                            impl_->data.seed, false);
                if (track->warmTime + 1e-6f >= track->warmEnd) {
                    track->lastBirth -= track->warmEnd;
                    track->warmEnd = 0;
                }
                warming = true;
            }
        }
        if (!warming) {
            break;
        }
    }
    if (warming) {
        impl_->endQuery(query);
        return;
    }
    const sf::Vector2f position = impl_->host.transformPoint({0, 0});
    if (impl_->hasPosition) {
        impl_->accumulatedMotion += position - impl_->previousPosition;
    }
    impl_->previousPosition = position;
    impl_->hasPosition = true;
    impl_->remainder += static_cast<double>(deltaTime) * impl_->speed;
    const int steps = static_cast<int>(
        std::min(240.0, std::floor((impl_->remainder + 1e-8) / step)));
    const sf::Vector2f motion =
        steps > 0 ? impl_->accumulatedMotion / static_cast<float>(steps)
                  : sf::Vector2f{};
    for (int index = 0; index < steps; ++index) {
        const float previous = static_cast<float>(impl_->time);
        impl_->time += step;
        for (auto& track : impl_->tracks) {
            track->step(previous, static_cast<float>(impl_->time),
                        static_cast<float>(step), motion.length(), motion,
                        impl_->host, impl_->data.seed, impl_->draining);
        }
        impl_->remainder -= step;
    }
    if (steps > 0) {
        impl_->accumulatedMotion = {};
    }
    impl_->endQuery(query);
    bool finished = true;
    for (const auto& track : impl_->tracks) {
        const auto& data = track->definition;
        const float end =
            impl_->draining
                ? track->lastBirth
                : std::max(track->lastBirth, data.delay + data.duration);
        if ((!impl_->draining && data.loop) ||
            impl_->time < end + data.lifetime.y) {
            finished = false;
            break;
        }
    }
    if (finished) {
        impl_->finished = true;
        impl_->playing = false;
    }
}
void GpuEmitterBackend::draw(sf::RenderTarget& target,
                             sf::RenderStates states) {
    if (impl_->data.tracks.empty()) {
        return;
    }
    const bool invisible = impl_->finished || (!impl_->playing && impl_->reset);
    if (invisible && (!impl_->profiling || !impl_->resources)) {
        return;
    }
    if (!target.setActive(true)) {
        throw std::runtime_error("Cannot activate Emitter render target");
    }
    impl_->prepare();
    ludork::runtime::graphics::GpuStateGuard guard(impl_->resources->api);
    Impl::Query* query = impl_->beginQuery(false);
    if (!invisible) {
        for (auto& track : impl_->tracks) {
            track->draw(target, states, impl_->host, impl_->colour);
        }
    }
    impl_->sampleStatistics();
    impl_->endQuery(query);
}
void GpuEmitterBackend::shutdown() noexcept {
    if (!impl_ || !impl_->resources) {
        return;
    }
    auto retired = std::make_unique<GpuEmitterBackend>();
    retired->impl_->resources = std::move(impl_->resources);
    retired->impl_->tracks = std::move(impl_->tracks);
    retired->impl_->statistics = std::move(impl_->statistics);
    impl_->sample.reset();
    retired->impl_->queries = impl_->queries;
    impl_->queries = {};
    impl_->reset = true;
    impl_->playing = false;
    const std::lock_guard lock(retirementMutex());
    retiredEmitters().push_back(std::move(retired));
}
void GpuEmitterBackend::collectGarbage() noexcept {
    const std::uint64_t contextId = sf::Context::getActiveContextId();
    if (contextId == 0) {
        return;
    }
    std::vector<std::unique_ptr<GpuEmitterBackend>> retired;
    {
        const std::lock_guard lock(retirementMutex());
        auto& pending = retiredEmitters();
        for (auto entry = pending.begin(); entry != pending.end();) {
            if ((*entry)->impl_->resources->contextId == contextId) {
                retired.push_back(std::move(*entry));
                entry = pending.erase(entry);
            } else {
                ++entry;
            }
        }
    }
    for (auto& emitter : retired) {
        for (const Impl::Query& query : emitter->impl_->queries) {
            if (query.id) {
                emitter->impl_->resources->api.DeleteQueries(1, &query.id);
            }
        }
        emitter->impl_->statistics.reset();
        emitter->impl_->tracks.clear();
        emitter->impl_->resources.reset();
    }
}
}  // namespace ludork::runtime::graphics
