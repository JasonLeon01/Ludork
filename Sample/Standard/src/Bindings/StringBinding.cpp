#include "Bindings.hpp"

#include "Core/Utf8.hpp"

#include <sol2/sol.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ludork::standard::binding {

namespace {

using detail::Utf8Codepoint;
using detail::decodeUtf8Codepoint;
using detail::validateUtf8;

enum class FormatPartKind {
    Text,
    Positional,
    Named,
};

struct FormatPart {
    FormatPartKind kind;
    std::string value;
};

struct ParsedFormat {
    std::vector<FormatPart> parts;
    bool hasNamed = false;
};

bool isJavaWhitespace(char32_t codepoint) {
    return (codepoint >= U'\u0009' && codepoint <= U'\u000D') ||
           (codepoint >= U'\u001C' && codepoint <= U'\u001F') ||
           codepoint == U'\u0020' || codepoint == U'\u1680' ||
           (codepoint >= U'\u2000' && codepoint <= U'\u2006') ||
           (codepoint >= U'\u2008' && codepoint <= U'\u200A') ||
           (codepoint >= U'\u2028' && codepoint <= U'\u2029') ||
           codepoint == U'\u205F' || codepoint == U'\u3000';
}

bool isIdentifier(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    const auto isLetter = [](char character) {
        return (character >= 'A' && character <= 'Z') ||
               (character >= 'a' && character <= 'z') || character == '_';
    };
    const auto isDigit = [](char character) {
        return character >= '0' && character <= '9';
    };
    if (!isLetter(value.front())) {
        return false;
    }
    for (std::size_t index = 1; index < value.size(); ++index) {
        if (!isLetter(value[index]) && !isDigit(value[index])) {
            return false;
        }
    }
    return true;
}

ParsedFormat parseFormat(const std::string& format) {
    validateUtf8(format, "format");
    ParsedFormat result;
    std::string text;
    const auto flushText = [&result, &text]() {
        if (text.empty()) {
            return;
        }
        result.parts.push_back({FormatPartKind::Text, std::move(text)});
        text.clear();
    };
    std::size_t index = 0;
    while (index < format.size()) {
        const char character = format[index];
        if (character == '{') {
            if (index + 1 < format.size() && format[index + 1] == '{') {
                text.push_back('{');
                index += 2;
                continue;
            }
            flushText();
            std::size_t close = index + 1;
            while (close < format.size() && format[close] != '}') {
                if (format[close] == '{') {
                    throw std::invalid_argument(
                        "pformat contains a nested opening brace");
                }
                ++close;
            }
            if (close == format.size()) {
                throw std::invalid_argument(
                    "pformat contains an unmatched opening brace");
            }
            const std::string field =
                format.substr(index + 1, close - index - 1);
            if (field.empty()) {
                result.parts.push_back({FormatPartKind::Positional, {}});
            } else {
                if (!isIdentifier(field)) {
                    throw std::invalid_argument(
                        "pformat field must be empty or an ASCII identifier");
                }
                result.parts.push_back({FormatPartKind::Named, field});
                result.hasNamed = true;
            }
            index = close + 1;
            continue;
        }
        if (character == '}') {
            if (index + 1 < format.size() && format[index + 1] == '}') {
                text.push_back('}');
                index += 2;
                continue;
            }
            throw std::invalid_argument(
                "pformat contains an unmatched closing brace");
        }
        text.push_back(character);
        ++index;
    }
    flushText();
    return result;
}

std::string stringify(const sol::object& value,
                      sol::protected_function& toString) {
    sol::protected_function_result converted = toString(value);
    if (!converted.valid()) {
        const sol::error error = converted;
        throw std::invalid_argument(std::string("pformat tostring failed: ") +
                                    error.what());
    }
    const sol::object result = converted.get<sol::object>();
    if (!result.is<std::string>()) {
        throw std::invalid_argument("pformat tostring did not return a string");
    }
    std::string text = result.as<std::string>();
    validateUtf8(text, "pformat value");
    return text;
}

std::string pformat(sol::this_state current, const std::string& format,
                    sol::variadic_args arguments) {
    sol::state_view lua(current);
    const sol::object rawToString =
        lua.globals().raw_get<sol::object>("tostring");
    if (!rawToString.is<sol::protected_function>()) {
        throw std::runtime_error("Lua tostring function is not defined");
    }
    sol::protected_function toString =
        rawToString.as<sol::protected_function>();
    const ParsedFormat parsed = parseFormat(format);
    const std::size_t argumentCount = arguments.size();
    std::size_t positionalCount = argumentCount;
    sol::table mapping;
    if (parsed.hasNamed) {
        if (argumentCount == 0 ||
            arguments.get_type(argumentCount - 1) != sol::type::table) {
            throw std::invalid_argument(
                "pformat named fields require a final mapping table");
        }
        mapping = arguments[argumentCount - 1].get<sol::table>();
        positionalCount -= 1;
    }

    std::string result;
    std::size_t positionalIndex = 0;
    for (const FormatPart& part : parsed.parts) {
        if (part.kind == FormatPartKind::Text) {
            result += part.value;
            continue;
        }
        if (part.kind == FormatPartKind::Positional) {
            if (positionalIndex >= positionalCount) {
                throw std::invalid_argument(
                    "pformat is missing a positional argument");
            }
            const sol::object value =
                arguments[positionalIndex].get<sol::object>();
            result += stringify(value, toString);
            ++positionalIndex;
            continue;
        }
        const sol::object value =
            mapping.raw_get<sol::object>(part.value);
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            throw std::invalid_argument("pformat mapping is missing key '" +
                                        part.value + "'");
        }
        result += stringify(value, toString);
    }
    return result;
}

std::string strip(const std::string& value, bool leading, bool trailing) {
    std::size_t begin = 0;
    std::size_t end = trailing ? 0 : value.size();
    bool leadingWhitespace = leading;
    std::size_t offset = 0;
    while (offset < value.size()) {
        const Utf8Codepoint codepoint =
            decodeUtf8Codepoint(value, "string", offset);
        const std::size_t next = offset + codepoint.length;
        const bool whitespace = isJavaWhitespace(codepoint.value);
        if (leadingWhitespace) {
            if (whitespace) {
                begin = next;
            } else {
                leadingWhitespace = false;
            }
        }
        if (trailing && !whitespace) {
            end = next;
        }
        offset = next;
    }
    if (end < begin) {
        return {};
    }
    return value.substr(begin, end - begin);
}

std::string replaceLiteral(const std::string& value,
                           const std::string& target,
                           const std::string& replacement) {
    validateUtf8(value, "string");
    validateUtf8(target, "target");
    validateUtf8(replacement, "replacement");
    std::string result;
    if (target.empty()) {
        result += replacement;
        std::size_t offset = 0;
        while (offset < value.size()) {
            const Utf8Codepoint codepoint =
                decodeUtf8Codepoint(value, "string", offset);
            result.append(value, codepoint.offset, codepoint.length);
            result += replacement;
            offset += codepoint.length;
        }
        return result;
    }
    std::size_t start = 0;
    while (true) {
        const std::size_t found = value.find(target, start);
        if (found == std::string::npos) {
            result.append(value, start, std::string::npos);
            return result;
        }
        result.append(value, start, found - start);
        result += replacement;
        start = found + target.size();
    }
}

std::vector<std::string> splitLiteral(const std::string& value,
                                      const std::string& separator) {
    validateUtf8(value, "string");
    validateUtf8(separator, "separator");
    if (separator.empty()) {
        throw std::invalid_argument("split separator must not be empty");
    }
    std::vector<std::string> result;
    std::size_t start = 0;
    while (true) {
        const std::size_t found = value.find(separator, start);
        if (found == std::string::npos) {
            result.push_back(value.substr(start));
            return result;
        }
        result.push_back(value.substr(start, found - start));
        start = found + separator.size();
    }
}

std::vector<std::size_t> utf8Offsets(const std::string& value) {
    std::vector<std::size_t> result{0};
    std::size_t offset = 0;
    while (offset < value.size()) {
        offset += decodeUtf8Codepoint(value, "string", offset).length;
        result.push_back(offset);
    }
    return result;
}

lua_Integer normaliseUtf8SliceIndex(lua_Integer index,
                                    lua_Integer length) {
    if (index < 0) {
        if (index < -length) {
            return 0;
        }
        index += length;
    }
    if (index < 0) {
        return 0;
    }
    if (index > length) {
        return length;
    }
    return index;
}

lua_Integer utf8Length(const std::string& value) {
    return static_cast<lua_Integer>(utf8Offsets(value).size() - 1);
}

std::string utf8Slice(const std::string& value, lua_Integer start,
                      lua_Integer finish) {
    const std::vector<std::size_t> offsets = utf8Offsets(value);
    const lua_Integer length =
        static_cast<lua_Integer>(offsets.size() - 1);
    const lua_Integer first = normaliseUtf8SliceIndex(start, length);
    const lua_Integer last = normaliseUtf8SliceIndex(finish, length);
    if (last <= first) {
        return {};
    }
    const std::size_t firstOffset =
        offsets[static_cast<std::size_t>(first)];
    const std::size_t lastOffset =
        offsets[static_cast<std::size_t>(last)];
    return value.substr(firstOffset, lastOffset - firstOffset);
}

}  // namespace

void registerString(sol::state_view lua) {
    const sol::object rawString =
        lua.globals().raw_get<sol::object>("string");
    if (!rawString.is<sol::table>()) {
        throw std::runtime_error("Lua string library is not defined");
    }
    sol::table stringTable = rawString.as<sol::table>();
    stringTable.set_function(
        "pformat",
        [](sol::this_state current, const std::string& format,
           sol::variadic_args arguments) {
            return pformat(current, format, arguments);
        });
    stringTable.set_function(
        "contains",
        [](const std::string& value, const std::string& target) {
            validateUtf8(value, "string");
            validateUtf8(target, "target");
            return value.find(target) != std::string::npos;
        });
    stringTable.set_function(
        "startsWith",
        [](const std::string& value, const std::string& prefix) {
            validateUtf8(value, "string");
            validateUtf8(prefix, "prefix");
            return value.starts_with(prefix);
        });
    stringTable.set_function(
        "endsWith",
        [](const std::string& value, const std::string& suffix) {
            validateUtf8(value, "string");
            validateUtf8(suffix, "suffix");
            return value.ends_with(suffix);
        });
    stringTable.set_function("isEmpty", [](const std::string& value) {
        validateUtf8(value, "string");
        return value.empty();
    });
    stringTable.set_function("isBlank", [](const std::string& value) {
        bool blank = true;
        std::size_t offset = 0;
        while (offset < value.size()) {
            const Utf8Codepoint codepoint =
                decodeUtf8Codepoint(value, "string", offset);
            if (!isJavaWhitespace(codepoint.value)) {
                blank = false;
            }
            offset += codepoint.length;
        }
        return blank;
    });
    stringTable.set_function("strip", [](const std::string& value) {
        return strip(value, true, true);
    });
    stringTable.set_function(
        "stripLeading", [](const std::string& value) {
            return strip(value, true, false);
        });
    stringTable.set_function(
        "stripTrailing", [](const std::string& value) {
            return strip(value, false, true);
        });
    stringTable.set_function("replace", &replaceLiteral);
    stringTable.set_function("split", [](const std::string& value,
                                         const std::string& separator) {
        return sol::as_table(splitLiteral(value, separator));
    });
    stringTable.set_function("utf8Length", &utf8Length);
    stringTable.set_function("utf8Slice", &utf8Slice);
}

}  // namespace ludork::standard::binding
