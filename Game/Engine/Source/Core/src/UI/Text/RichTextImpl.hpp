#pragma once

#include <UI/RichText.hpp>
#include <UI/TextEffects.hpp>

struct RichText::EffectCache {
    ludork::engine::text_effects::Cache data;
};
