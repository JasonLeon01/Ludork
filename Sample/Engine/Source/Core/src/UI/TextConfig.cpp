#include <UI/TextConfig.hpp>
#include <UI/PlainTextConfig.hpp>
#include <UI/RichText.hpp>

#include "Text/TextConfigCodec.hpp"

std::shared_ptr<PlainTextConfig> buildPlainTextConfig(
    const RuntimeValue::Map& data, const std::string& sourceName) {
    return ludork::engine::text_config::buildPlain(data, sourceName);
}

std::shared_ptr<RichText::RichTextConfig> buildRichTextConfig(
    const RuntimeValue::Map& data, const std::string& sourceName) {
    return ludork::engine::text_config::buildRich(data, sourceName);
}
