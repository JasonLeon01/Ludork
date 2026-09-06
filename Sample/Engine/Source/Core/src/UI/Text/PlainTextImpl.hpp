#pragma once

#include <UI/PlainText.hpp>
#include <UI/TextEffects.hpp>

struct PlainText::EffectCache {
    ludork::engine::text_effects::Cache data;
};
