#pragma once

#include <BackgroundTask.hpp>

#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten/fiber.h>
#include <array>
#include <cstddef>
#else
#include <thread>
#endif

namespace ludork::standard {

struct BackgroundTask::Impl {
    explicit Impl(std::function<void()> work);
    void resume();
#ifdef __EMSCRIPTEN__
    static void enter(void* value);
    static std::vector<std::weak_ptr<Impl>>& tasks();
    static Impl* active;
    static emscripten_fiber_t mainFiber;
    alignas(16) static std::array<std::byte, 65536> mainStack;
    std::function<void()> work;
    static constexpr std::size_t CStackSize = 1024 * 1024;
    std::vector<std::byte> stack;
    std::vector<std::byte> asyncifyStack;
    emscripten_fiber_t fiber{};
    emscripten_fiber_t* caller = nullptr;
    bool finished = false;
    bool joined = false;
#else
    std::thread worker;
#endif
};

}  // namespace ludork::standard
