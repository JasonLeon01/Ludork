#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <UI/Text.hpp>

#include <string>

BIND_FUNCTION_GROUP(name = "TextLayout")

BIND_FUNCTION(metadata = false)
float measurePlainText(PlainText& control, const std::string& text);

BIND_FUNCTION(metadata = false)
float measureRichText(const std::string& textConfigKey,
                      const std::string& text);

BIND_FUNCTION(metadata = false)
std::string fitPlainText(const std::string& text, float maxWidth,
                         PlainText& control);

BIND_FUNCTION(metadata = false)
std::string wrapPlainText(const std::string& text, float maxWidth,
                          PlainText& control);

BIND_FUNCTION(metadata = false)
std::string wrapRichText(const std::string& text, float maxWidth,
                         const std::string& textConfigKey);
