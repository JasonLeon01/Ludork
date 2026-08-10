#include <Utils/File.hpp>

#include <DataFile.hpp>
#include <Utf8Path.hpp>

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    RuntimeValue parse() {
        skipWhitespace();
        RuntimeValue result = parseValue();
        skipWhitespace();
        if (position_ != text_.size()) {
            fail("Unexpected trailing JSON data");
        }
        return result;
    }

private:
    RuntimeValue parseValue() {
        if (position_ >= text_.size()) {
            fail("Unexpected end of JSON input");
        }
        switch (text_[position_]) {
            case 'n':
                consumeLiteral("null");
                return RuntimeValue();
            case 't':
                consumeLiteral("true");
                return RuntimeValue(true);
            case 'f':
                consumeLiteral("false");
                return RuntimeValue(false);
            case '"':
                return RuntimeValue(parseString());
            case '[':
                return RuntimeValue(parseArray());
            case '{':
                return RuntimeValue(parseObject());
            default:
                return parseNumber();
        }
    }

    RuntimeValue::Array parseArray() {
        ++position_;
        skipWhitespace();
        RuntimeValue::Array result;
        if (consume(']')) {
            return result;
        }
        while (true) {
            skipWhitespace();
            result.push_back(parseValue());
            skipWhitespace();
            if (consume(']')) {
                return result;
            }
            require(',');
        }
    }

    RuntimeValue::Map parseObject() {
        ++position_;
        skipWhitespace();
        RuntimeValue::Map result;
        if (consume('}')) {
            return result;
        }
        while (true) {
            skipWhitespace();
            if (position_ >= text_.size() || text_[position_] != '"') {
                fail("JSON object key must be a string");
            }
            std::string key = parseString();
            skipWhitespace();
            require(':');
            skipWhitespace();
            result.insert_or_assign(std::move(key), parseValue());
            skipWhitespace();
            if (consume('}')) {
                return result;
            }
            require(',');
        }
    }

    RuntimeValue parseNumber() {
        const std::size_t start = position_;
        consume('-');
        if (consume('0')) {
        } else {
            consumeDigits();
        }
        bool floatingPoint = false;
        if (consume('.')) {
            floatingPoint = true;
            consumeDigits();
        }
        if (position_ < text_.size() &&
            (text_[position_] == 'e' || text_[position_] == 'E')) {
            floatingPoint = true;
            ++position_;
            if (position_ < text_.size() &&
                (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            consumeDigits();
        }
        if (start == position_) {
            fail("Invalid JSON value");
        }
        const std::string token = text_.substr(start, position_ - start);
        if (floatingPoint) {
            const double value = std::stod(token);
            if (!std::isfinite(value)) {
                fail("JSON number must be finite");
            }
            return RuntimeValue(value);
        }
        return RuntimeValue(static_cast<std::int64_t>(std::stoll(token)));
    }

    std::string parseString() {
        require('"');
        std::string result;
        while (position_ < text_.size()) {
            const unsigned char character =
                static_cast<unsigned char>(text_[position_++]);
            if (character == '"') {
                return result;
            }
            if (character < 0x20u) {
                fail("Unescaped control character in JSON string");
            }
            if (character != '\\') {
                result.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ >= text_.size()) {
                fail("Invalid JSON escape sequence");
            }
            const char escape = text_[position_++];
            switch (escape) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escape);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u':
                    appendUtf8(result, parseUnicodeEscape());
                    break;
                default:
                    fail("Invalid JSON escape sequence");
            }
        }
        fail("Unterminated JSON string");
    }

    char32_t parseUnicodeEscape() {
        char32_t codepoint = parseHexQuad();
        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (position_ + 2 > text_.size() || text_[position_] != '\\' ||
                text_[position_ + 1] != 'u') {
                fail("Missing low surrogate in JSON string");
            }
            position_ += 2;
            const char32_t low = parseHexQuad();
            if (low < 0xdc00 || low > 0xdfff) {
                fail("Invalid low surrogate in JSON string");
            }
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
            fail("Unexpected low surrogate in JSON string");
        }
        return codepoint;
    }

    char32_t parseHexQuad() {
        if (position_ + 4 > text_.size()) {
            fail("Incomplete JSON unicode escape");
        }
        char32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            const char character = text_[position_++];
            value <<= 4;
            if (character >= '0' && character <= '9') {
                value += character - '0';
            } else if (character >= 'a' && character <= 'f') {
                value += character - 'a' + 10;
            } else if (character >= 'A' && character <= 'F') {
                value += character - 'A' + 10;
            } else {
                fail("Invalid JSON unicode escape");
            }
        }
        return value;
    }

    static void appendUtf8(std::string& target, char32_t codepoint) {
        if (codepoint <= 0x7f) {
            target.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            target.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            target.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            target.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            target.push_back(
                static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            target.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            target.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            target.push_back(
                static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            target.push_back(
                static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            target.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    void consumeLiteral(const std::string& literal) {
        if (text_.compare(position_, literal.size(), literal) != 0) {
            fail("Invalid JSON literal");
        }
        position_ += literal.size();
    }

    void consumeDigits() {
        const std::size_t start = position_;
        while (position_ < text_.size() && text_[position_] >= '0' &&
               text_[position_] <= '9') {
            ++position_;
        }
        if (start == position_) {
            fail("JSON number requires digits");
        }
    }

    bool consume(char character) {
        if (position_ < text_.size() && text_[position_] == character) {
            ++position_;
            return true;
        }
        return false;
    }

    void require(char character) {
        if (!consume(character)) {
            fail(std::string("Expected '") + character + "' in JSON input");
        }
    }

    void skipWhitespace() {
        while (position_ < text_.size() &&
               (text_[position_] == ' ' || text_[position_] == '\t' ||
                text_[position_] == '\r' || text_[position_] == '\n')) {
            ++position_;
        }
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error(message + " at byte " +
                                 std::to_string(position_));
    }

    std::string text_;
    std::size_t position_ = 0;
};

void writeEscapedString(std::ostream& output, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    output.put('"');
    for (const unsigned char character : value) {
        switch (character) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (character < 0x20u) {
                    output << "\\u00" << hex[(character >> 4u) & 0xfu]
                           << hex[character & 0xfu];
                } else {
                    output.put(static_cast<char>(character));
                }
        }
    }
    output.put('"');
}

void writeValue(std::ostream& output, const RuntimeValue& value) {
    if (value.isNil()) {
        output << "null";
    } else if (const bool* boolean = value.getIf<bool>()) {
        output << (*boolean ? "true" : "false");
    } else if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        output << *integer;
    } else if (const double* number = value.getIf<double>()) {
        if (!std::isfinite(*number)) {
            throw std::invalid_argument("JSON number must be finite");
        }
        output << std::setprecision(std::numeric_limits<double>::max_digits10)
               << *number;
    } else if (const std::string* text = value.getIf<std::string>()) {
        writeEscapedString(output, *text);
    } else if (const RuntimeValue::Array* array =
                   value.getIf<RuntimeValue::Array>()) {
        output.put('[');
        for (std::size_t index = 0; index < array->size(); ++index) {
            if (index != 0) {
                output.put(',');
            }
            writeValue(output, (*array)[index]);
        }
        output.put(']');
    } else if (const RuntimeValue::Map* map =
                   value.getIf<RuntimeValue::Map>()) {
        output.put('{');
        bool first = true;
        for (const auto& [key, item] : *map) {
            if (!first) {
                output.put(',');
            }
            first = false;
            writeEscapedString(output, key);
            output.put(':');
            writeValue(output, item);
        }
        output.put('}');
    } else {
        throw std::invalid_argument("Runtime object is not JSON serializable");
    }
}

}  // namespace

RuntimeValue getJSONData(const std::string& filePath) {
    return getJSONData(ludork::standard::pathFromUtf8(filePath));
}

RuntimeValue getJSONData(const std::filesystem::path& filePath) {
    return parseJSONText(getJSONText(filePath));
}

RuntimeValue parseJSONText(const std::string& text) {
    return JsonParser(text).parse();
}

std::string stringifyJSON(const RuntimeValue& value) {
    std::ostringstream output;
    writeValue(output, value);
    if (!output) {
        throw std::runtime_error("Failed to serialize JSON value");
    }
    return output.str();
}

std::string getJSONText(const std::string& filePath) {
    return getJSONText(ludork::standard::pathFromUtf8(filePath));
}

std::string getJSONText(const std::filesystem::path& filePath) {
    return ludork::standard::readJsonText(filePath);
}

bool jsonExists(const std::string& filePath) {
    return jsonExists(ludork::standard::pathFromUtf8(filePath));
}

bool jsonExists(const std::filesystem::path& filePath) {
    return ludork::standard::jsonDataExists(filePath);
}

void writeJSON(const std::string& filePath, const RuntimeValue& value) {
    writeJSON(ludork::standard::pathFromUtf8(filePath), value);
}

void writeJSON(const std::filesystem::path& filePath,
               const RuntimeValue& value) {
    ludork::standard::writeJsonText(filePath, stringifyJSON(value));
}
