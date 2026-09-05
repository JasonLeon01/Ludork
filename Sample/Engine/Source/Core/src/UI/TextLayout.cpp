#include <UI/TextLayout.hpp>

#include "Text/TextConfigCodec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct TextUnit {
    std::string value;
    bool marker = false;
};

using MeasureFunction = std::function<float(const std::string&)>;

std::size_t nextUtf8Index(const std::string& text, std::size_t index) {
    const unsigned char first = static_cast<unsigned char>(text[index]);
    std::size_t length = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if (first <= 0x7FU) {
        return index + 1;
    }
    if ((first & 0xE0U) == 0xC0U) {
        length = 2;
        value = first & 0x1FU;
        minimum = 0x80U;
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
        value = first & 0x0FU;
        minimum = 0x800U;
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
        value = first & 0x07U;
        minimum = 0x10000U;
    } else {
        throw std::invalid_argument("Text contains invalid UTF-8");
    }
    if (index + length > text.size()) {
        throw std::invalid_argument("Text contains truncated UTF-8");
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        const unsigned char continuation =
            static_cast<unsigned char>(text[index + offset]);
        if ((continuation & 0xC0U) != 0x80U) {
            throw std::invalid_argument("Text contains invalid UTF-8");
        }
        value = (value << 6U) | (continuation & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFFU ||
        (value >= 0xD800U && value <= 0xDFFFU)) {
        throw std::invalid_argument("Text contains invalid UTF-8");
    }
    return index + length;
}

std::vector<TextUnit> plainUnits(const std::string& text) {
    std::vector<TextUnit> result;
    for (std::size_t index = 0; index < text.size();) {
        const std::size_t next = nextUtf8Index(text, index);
        result.push_back({text.substr(index, next - index), false});
        index = next;
    }
    return result;
}

std::vector<TextUnit> richUnits(
    RichText& control, const std::string& text,
    std::unordered_map<std::string, bool>& markerFlags) {
    std::vector<TextUnit> result;
    for (std::size_t index = 0; index < text.size();) {
        if (text[index] == '#') {
            const std::size_t markerEnd = text.find('#', index + 1);
            if (markerEnd != std::string::npos) {
                const std::string marker =
                    text.substr(index, markerEnd - index + 1);
                auto [iterator, inserted] = markerFlags.emplace(marker, false);
                if (inserted) {
                    control.setString(marker);
                    iterator->second = control.getLocalBounds().size.x == 0.0f;
                }
                result.push_back({marker, iterator->second});
                index = markerEnd + 1;
                continue;
            }
        }
        const std::size_t next = nextUtf8Index(text, index);
        result.push_back({text.substr(index, next - index), false});
        index = next;
    }
    return result;
}

std::string concatenate(const std::vector<TextUnit>& units, std::size_t first,
                        std::size_t last) {
    std::string result;
    for (std::size_t index = first; index < last; ++index) {
        result += units[index].value;
    }
    return result;
}

std::string concatenateMarkers(const std::vector<TextUnit>& units,
                               std::size_t first, std::size_t last) {
    std::string result;
    for (std::size_t index = first; index < last; ++index) {
        if (units[index].marker) {
            result += units[index].value;
        }
    }
    return result;
}

std::size_t visibleCount(const std::vector<TextUnit>& units, std::size_t first,
                         std::size_t last) {
    return static_cast<std::size_t>(
        std::count_if(units.begin() + static_cast<std::ptrdiff_t>(first),
                      units.begin() + static_cast<std::ptrdiff_t>(last),
                      [](const TextUnit& unit) {
                          return !unit.marker;
                      }));
}

std::size_t firstVisibleEnd(const std::vector<TextUnit>& units,
                            std::size_t first) {
    std::size_t end = first;
    while (end < units.size()) {
        ++end;
        if (!units[end - 1].marker) {
            break;
        }
    }
    return end;
}

std::size_t maximumFittingEnd(const std::vector<TextUnit>& units,
                              std::size_t first,
                              const std::string& markerPrefix, float maxWidth,
                              const MeasureFunction& measure) {
    if (first == units.size()) {
        return first;
    }
    std::size_t low = first;
    std::size_t high = units.size();
    while (low < high) {
        const std::size_t middle = low + (high - low + 1) / 2;
        const float width =
            measure(markerPrefix + concatenate(units, first, middle));
        if (width <= maxWidth) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return low;
}

std::string wrapParagraph(std::vector<TextUnit> units, float maxWidth,
                          std::string& activeMarkers,
                          const MeasureFunction& measure) {
    std::vector<std::string> lines;
    std::size_t first = 0;
    std::string linePrefix = activeMarkers;
    while (first < units.size()) {
        std::size_t fitting =
            maximumFittingEnd(units, first, linePrefix, maxWidth, measure);
        if (fitting == units.size()) {
            lines.push_back(concatenate(units, first, units.size()));
            linePrefix += concatenateMarkers(units, first, units.size());
            first = units.size();
            break;
        }

        const std::size_t firstVisible = firstVisibleEnd(units, first);
        if (fitting < firstVisible) {
            fitting = firstVisible;
        }
        std::size_t overflow = fitting;
        while (overflow < units.size() && units[overflow].marker) {
            ++overflow;
        }
        if (overflow < units.size()) {
            ++overflow;
        }

        std::size_t space = overflow;
        while (space > first) {
            --space;
            if (!units[space].marker && units[space].value == " ") {
                break;
            }
        }
        const bool foundSpace = space >= first && space < overflow &&
                                !units[space].marker &&
                                units[space].value == " ";
        if (foundSpace) {
            if (visibleCount(units, first, space) > 0) {
                lines.push_back(concatenate(units, first, space));
            }
            linePrefix += concatenateMarkers(units, first, space + 1);
            first = space + 1;
            std::size_t scan = first;
            while (scan < units.size()) {
                if (units[scan].marker) {
                    ++scan;
                } else if (units[scan].value == " ") {
                    units.erase(units.begin() +
                                static_cast<std::ptrdiff_t>(scan));
                } else {
                    break;
                }
            }
            continue;
        }

        const std::size_t lineEnd = std::max(fitting, firstVisible);
        lines.push_back(concatenate(units, first, lineEnd));
        linePrefix += concatenateMarkers(units, first, lineEnd);
        first = lineEnd;
    }
    if (units.empty()) {
        lines.emplace_back();
    }
    activeMarkers = linePrefix;
    std::string result;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            result.push_back('\n');
        }
        result += lines[index];
    }
    return result;
}

std::string wrapText(
    const std::string& text, float maxWidth, const MeasureFunction& measure,
    const std::function<std::vector<TextUnit>(const std::string&)>& tokenize) {
    if (text.empty()) {
        return {};
    }
    if (!std::isfinite(maxWidth) || maxWidth <= 0.0f) {
        throw std::invalid_argument(
            "Text wrap width must be a positive finite number");
    }

    std::string result;
    std::string activeMarkers;
    std::size_t paragraphStart = 0;
    bool firstParagraph = true;
    while (paragraphStart <= text.size()) {
        const std::size_t paragraphEnd = text.find('\n', paragraphStart);
        const std::size_t end =
            paragraphEnd == std::string::npos ? text.size() : paragraphEnd;
        if (!firstParagraph) {
            result.push_back('\n');
        }
        firstParagraph = false;
        result += wrapParagraph(
            tokenize(text.substr(paragraphStart, end - paragraphStart)),
            maxWidth, activeMarkers, measure);
        if (paragraphEnd == std::string::npos) {
            break;
        }
        paragraphStart = paragraphEnd + 1;
    }
    return result;
}

class PlainTextMeasurement {
public:
    explicit PlainTextMeasurement(PlainText& control)
        : control_(control), previous_(control.getString()) {}

    ~PlainTextMeasurement() {
        control_.setString(previous_);
    }

    float operator()(const std::string& value) {
        const auto existing = cache_.find(value);
        if (existing != cache_.end()) {
            return existing->second;
        }
        control_.setString(value);
        const float width = control_.getLocalBounds().size.x;
        cache_.emplace(value, width);
        return width;
    }

private:
    PlainText& control_;
    std::string previous_;
    std::unordered_map<std::string, float> cache_;
};

}  // namespace

float measurePlainText(PlainText& control, const std::string& text) {
    PlainTextMeasurement measurement(control);
    return measurement(text);
}

float measureRichText(const std::string& textConfigKey,
                      const std::string& text) {
    RichText control(ludork::engine::text_config::loadRich(textConfigKey),
                     text);
    return control.getLocalBounds().size.x;
}

std::string fitPlainText(const std::string& text, float maxWidth,
                         PlainText& control) {
    if (text.empty()) {
        return {};
    }
    const std::vector<TextUnit> units = plainUnits(text);
    PlainTextMeasurement measurement(control);
    std::size_t low = 0;
    std::size_t high = units.size();
    while (low < high) {
        const std::size_t middle = low + (high - low + 1) / 2;
        if (measurement(concatenate(units, 0, middle)) <= maxWidth) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    if (low == units.size()) {
        return text;
    }
    if (low > 1) {
        return concatenate(units, 0, low - 1) + ".";
    }
    return concatenate(units, 0, low);
}

std::string wrapPlainText(const std::string& text, float maxWidth,
                          PlainText& control) {
    PlainTextMeasurement measurement(control);
    return wrapText(
        text, maxWidth,
        [&](const std::string& value) {
            return measurement(value);
        },
        plainUnits);
}

std::string wrapRichText(const std::string& text, float maxWidth,
                         const std::string& textConfigKey) {
    RichText control(ludork::engine::text_config::loadRich(textConfigKey), "");
    std::unordered_map<std::string, float> measurements;
    std::unordered_map<std::string, bool> markerFlags;
    const MeasureFunction measure = [&](const std::string& value) {
        const auto existing = measurements.find(value);
        if (existing != measurements.end()) {
            return existing->second;
        }
        control.setString(value);
        const float width = control.getLocalBounds().size.x;
        measurements.emplace(value, width);
        return width;
    };
    return wrapText(text, maxWidth, measure, [&](const std::string& paragraph) {
        return richUnits(control, paragraph, markerFlags);
    });
}
