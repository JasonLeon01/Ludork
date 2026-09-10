#pragma once

#include <CoreMinimal.hpp>
#include <Runtime/RuntimeData.hpp>

#include <UI/PlainTextConfig.hpp>
#include <UI/RichText.hpp>

BIND_FUNCTION_GROUP(name = "TextConfig")

BIND_FUNCTION(name = "buildPlain", metadata = false)
std::shared_ptr<PlainTextConfig> buildPlainTextConfig(
    const RuntimeData::Map& data, const std::string& sourceName);

BIND_FUNCTION(name = "buildRich", metadata = false)
std::shared_ptr<RichText::RichTextConfig> buildRichTextConfig(
    const RuntimeData::Map& data, const std::string& sourceName);
