#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct MapPanoramaSettings {
    BIND_PROPERTY()
    std::string panorama;
};
