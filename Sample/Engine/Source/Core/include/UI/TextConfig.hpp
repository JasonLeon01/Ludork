#pragma once

#include <CoreMinimal.hpp>

#include <UI/Text.hpp>

BIND_FUNCTION_GROUP(name = "TextConfig")

BIND_FUNCTION(name = "buildPlain", metadata = false)
std::shared_ptr<PlainTextConfig> buildPlainTextConfig(
    const RuntimeValue::Map& data, const std::string& sourceName);

BIND_FUNCTION(name = "buildRich", metadata = false)
std::shared_ptr<RichTextConfig> buildRichTextConfig(
    const RuntimeValue::Map& data, const std::string& sourceName);
