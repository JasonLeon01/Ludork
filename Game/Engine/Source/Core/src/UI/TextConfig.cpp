#include <UI/TextConfig.hpp>
#include <UI/PlainTextConfig.hpp>
#include <UI/RichText.hpp>

#include <CoreShared/TextConfigCodec.hpp>

std::shared_ptr<PlainTextConfig> buildPlainTextConfig(
    const RuntimeData::Map& data, const std::string& sourceName) {
    return ludork::engine::text_config::buildPlain(data, sourceName);
}

std::shared_ptr<RichText::RichTextConfig> buildRichTextConfig(
    const RuntimeData::Map& data, const std::string& sourceName) {
    return ludork::engine::text_config::buildRich(data, sourceName);
}
