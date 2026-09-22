#pragma once

#include <CoreMinimal.hpp>

#include <UI/PlainText.hpp>
#include <UI/RichText.hpp>

BIND_FUNCTION_GROUP(name = "TextLayout")

BIND_FUNCTION(metadata = false)
float measurePlainText(PlainText& control, const std::string& text);

BIND_FUNCTION(metadata = false)
float measureRichText(RichText& source, const std::string& text);

BIND_FUNCTION(metadata = false)
std::string fitPlainText(const std::string& text, float maxWidth,
                         PlainText& control);

BIND_FUNCTION(metadata = false)
std::string wrapPlainText(const std::string& text, float maxWidth,
                          PlainText& control);

BIND_FUNCTION(metadata = false)
std::string wrapRichText(const std::string& text, float maxWidth,
                         RichText& source);
