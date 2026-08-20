#pragma once

#include <UI/UiControlAdapterRegistry.hpp>

struct UiControlAdapterRegistry::Builder {
    static void registerLayoutAdapters(UiControlAdapterRegistry& registry);
    static void registerVisualAdapters(UiControlAdapterRegistry& registry);
    static void registerInputAdapters(UiControlAdapterRegistry& registry);
    static void registerSkinnedAdapters(UiControlAdapterRegistry& registry);
    static void registerTextAdapters(UiControlAdapterRegistry& registry);
};
