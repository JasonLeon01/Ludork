#pragma once

#include <Runtime/RuntimeData.hpp>
#include <UI/PlainTextConfig.hpp>
#include <UI/RichText.hpp>

#include <SFML/Graphics/Font.hpp>

#include <memory>
#include <string>

namespace ludork::engine::text_config {

std::shared_ptr<sf::Font> loadFont(const std::string& fontKey,
                                   const std::string& source);

sf::Text::LineAlignment parseLineAlignment(const std::string& value,
                                           const std::string& source);

std::shared_ptr<PlainTextConfig> buildPlain(const RuntimeData::Map& data,
                                            const std::string& sourceName);

std::shared_ptr<RichText::RichTextConfig> buildRich(
    const RuntimeData::Map& data, const std::string& sourceName);

std::shared_ptr<PlainTextConfig> loadPlain(const std::string& textConfigKey);

std::shared_ptr<RichText::RichTextConfig> loadRich(
    const std::string& textConfigKey);

}  // namespace ludork::engine::text_config
