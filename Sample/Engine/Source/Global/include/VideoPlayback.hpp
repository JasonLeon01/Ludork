#pragma once

#include <cstdint>

struct lua_State;

void registerVideoPlayback(lua_State* state);
void processPendingVideoPlayback();
std::uint64_t getVideoPlaybackCompletionSequence() noexcept;
void shutdownVideoPlayback() noexcept;
