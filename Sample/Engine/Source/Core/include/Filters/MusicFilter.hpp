#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Filters/SoundFilter.hpp>

BIND_CLASS(table_init = true)
class MusicFilter : public SoundFilter {
public:
    BIND_PROPERTY()
    std::optional<sf::Music::TimeSpan> loopPoint;
};
