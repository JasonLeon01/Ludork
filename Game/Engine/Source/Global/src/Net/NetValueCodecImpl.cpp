#include "NetValueCodecImpl.hpp"

#include <Base64.hpp>
#include <Runtime/RuntimeDataReader.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace ludork::global::net_impl {
namespace {

RuntimeData scalar(const char* tag, RuntimeData value) {
    return RuntimeData(RuntimeData::Array{RuntimeData(tag), std::move(value)});
}

std::string decimal(double value) {
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (result.ec != std::errc{}) {
        throw std::invalid_argument("Cannot encode a network number.");
    }
    return std::string(buffer.data(), result.ptr);
}

std::int64_t parseInteger(const std::string& text) {
    std::int64_t result{};
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), result);
    if (text.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size()) {
        throw std::invalid_argument("Invalid network number.");
    }
    return result;
}

double parseFloat(const std::string& text) {
    if (text.empty() || text.front() == '+' ||
        text.find_first_not_of("0123456789.-+eE") != std::string::npos) {
        throw std::invalid_argument("Invalid network number.");
    }
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    double result{};
    input >> std::noskipws >> result;
    // libc++ can report a range failure for a valid subnormal result.
    const bool rangeFailure =
        input.fail() && std::fpclassify(result) != FP_SUBNORMAL;
    const bool underflow = result == 0 && text.find_first_of("123456789") <
                                              text.find_first_of("eE");
    if (!input.eof() || input.bad() || rangeFailure || underflow) {
        throw std::invalid_argument("Invalid network number.");
    }
    return result;
}

}  // namespace

void NetValueCodecImpl::consume(std::size_t depth) {
    if (depth > MaxDepth || ++nodes_ > MaxNodes) {
        throw std::invalid_argument(
            "Network value exceeds its depth or node limit.");
    }
}

void NetValueCodecImpl::consumeBytes(std::size_t bytes) {
    if (bytes > MaxMessageBytes - bytes_) {
        throw std::invalid_argument("Network value exceeds its byte limit.");
    }
    bytes_ += bytes;
}

RuntimeData NetValueCodecImpl::encode(const RuntimeIdentityPtr& value) {
    return encodeValue(RuntimeValue(value), 0);
}

RuntimeIdentityPtr NetValueCodecImpl::decode(const RuntimeData& value) {
    return ludork::runtime::reference::intern(decodeValue(value, 0)).identity();
}

RuntimeData NetValueCodecImpl::encodeValue(const RuntimeValue& original,
                                           std::size_t depth, bool key) {
    consume(depth);
    const RuntimeValue value = ludork::runtime::reference::retain(original);
    if (value.isNil()) {
        if (key) {
            throw std::invalid_argument("Network table keys cannot be nil.");
        }
        return RuntimeData(RuntimeData::Array{RuntimeData("nil")});
    }
    if (const bool* boolean = value.getIf<bool>()) {
        return scalar("bool", RuntimeData(*boolean));
    }
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return scalar("int", RuntimeData(std::to_string(*integer)));
    }
    if (const double* number = value.getIf<double>()) {
        if (!std::isfinite(*number)) {
            throw std::invalid_argument("Network numbers must be finite.");
        }
        return scalar("float", RuntimeData(decimal(*number)));
    }
    if (const std::string* text = value.getIf<std::string>()) {
        consumeBytes(text->size());
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(text->data());
        return scalar("string", RuntimeData(ludork::standard::encodeBase64(
                                    std::span(bytes, text->size()))));
    }
    if (key || !ludork::runtime::reference::isTable(value)) {
        throw std::invalid_argument(
            "Network values require nil, boolean, finite number, string or "
            "ordinary table; keys require boolean, number or string.");
    }
    const RuntimeHandle table = ludork::runtime::reference::intern(value);
    if (ludork::runtime::reference::hasMetatable(table)) {
        throw std::invalid_argument("Network tables cannot have metatables.");
    }
    if (std::any_of(active_.begin(), active_.end(),
                    [&table](const RuntimeIdentityPtr& identity) {
                        return identity->equals(*table.identity());
                    })) {
        throw std::invalid_argument("Network tables cannot contain cycles.");
    }
    active_.push_back(table.identity());
    RuntimeData::Array entries;
    for (const auto& [entryKey, entryValue] :
         ludork::runtime::reference::entries(table)) {
        RuntimeData encodedKey = encodeValue(entryKey, depth + 1, true);
        RuntimeData encodedValue = encodeValue(entryValue, depth + 1);
        entries.emplace_back(
            RuntimeData::Array{std::move(encodedKey), std::move(encodedValue)});
    }
    active_.pop_back();
    return scalar("table", RuntimeData(std::move(entries)));
}

RuntimeValue NetValueCodecImpl::decodeValue(const RuntimeData& value,
                                            std::size_t depth, bool key) {
    consume(depth);
    const RuntimeData::Array& node =
        ludork::runtime::value_reader::requireArray(value, "Network value");
    if (node.empty() || node.size() > 2) {
        throw std::invalid_argument("Invalid network value shape.");
    }
    const std::string& tag =
        ludork::runtime::value_reader::requireString(node[0], "Network tag");
    if (tag == "nil" && node.size() == 1 && !key) {
        return {};
    }
    if (node.size() != 2) {
        throw std::invalid_argument("Invalid network value payload.");
    }
    if (tag == "bool") {
        return RuntimeValue(ludork::runtime::value_reader::requireBool(
            node[1], "Network boolean"));
    }
    if (tag == "int") {
        const std::string& text = ludork::runtime::value_reader::requireString(
            node[1], "Network integer");
        const std::int64_t integer = parseInteger(text);
        if (std::to_string(integer) != text) {
            throw std::invalid_argument(
                "Network integers require canonical decimal spelling.");
        }
        return RuntimeValue(integer);
    }
    if (tag == "float") {
        const double number =
            parseFloat(ludork::runtime::value_reader::requireString(
                node[1], "Network float"));
        if (!std::isfinite(number)) {
            throw std::invalid_argument("Network numbers must be finite.");
        }
        return RuntimeValue(number);
    }
    if (tag == "string") {
        const std::string& text = ludork::runtime::value_reader::requireString(
            node[1], "Network string");
        if (text.size() > MaxMessageBytes) {
            throw std::invalid_argument(
                "Network string exceeds its byte limit.");
        }
        const std::vector<std::uint8_t> bytes =
            ludork::standard::decodeBase64(text);
        if (ludork::standard::encodeBase64(bytes) != text) {
            throw std::invalid_argument(
                "Network strings require canonical base64.");
        }
        consumeBytes(bytes.size());
        return RuntimeValue(std::string(bytes.begin(), bytes.end()));
    }
    if (tag != "table" || key) {
        throw std::invalid_argument("Invalid network value tag or table key.");
    }
    const RuntimeData::Array& entries =
        ludork::runtime::value_reader::requireArray(node[1], "Network table");
    const RuntimeHandle table = ludork::runtime::reference::table();
    for (const RuntimeData& entry : entries) {
        const RuntimeData::Array& pair =
            ludork::runtime::value_reader::requireArray(entry, "Network entry");
        if (pair.size() != 2) {
            throw std::invalid_argument("Invalid network table entry.");
        }
        const RuntimeValue entryKey = decodeValue(pair[0], depth + 1, true);
        const RuntimeValue entryValue = decodeValue(pair[1], depth + 1);
        if (entryValue.isNil() ||
            !ludork::runtime::reference::rawGet(table, entryKey).isNil()) {
            throw std::invalid_argument(
                "Network tables cannot contain nil entries or duplicate keys.");
        }
        ludork::runtime::reference::rawSet(table, entryKey, entryValue);
    }
    return RuntimeValue(table);
}

RuntimeData::Array NetValueCodecImpl::encodeArguments(
    const RuntimeIdentityPtr& arguments) {
    const RuntimeHandle packed =
        ludork::runtime::reference::requireTable(RuntimeValue(arguments));
    if (ludork::runtime::reference::hasMetatable(packed)) {
        throw std::invalid_argument(
            "Network arguments require an ordinary packed table.");
    }
    const RuntimeValue count = ludork::runtime::reference::rawGet(packed, "n");
    const std::int64_t* size = count.getIf<std::int64_t>();
    if (size == nullptr || *size < 0 ||
        static_cast<std::uint64_t>(*size) > MaxNodes) {
        throw std::invalid_argument(
            "Network arguments require a valid packed count.");
    }
    for (const auto& [key, value] :
         ludork::runtime::reference::entries(packed)) {
        static_cast<void>(value);
        if (const std::string* name = key.getIf<std::string>();
            name != nullptr && *name == "n") {
            continue;
        }
        const std::int64_t* index = key.getIf<std::int64_t>();
        if (index == nullptr || *index < 1 || *index > *size) {
            throw std::invalid_argument(
                "Network argument table has an invalid key.");
        }
    }
    RuntimeData::Array result;
    result.reserve(static_cast<std::size_t>(*size));
    for (std::int64_t index = 1; index <= *size; ++index) {
        result.push_back(
            encodeValue(ludork::runtime::reference::rawGet(packed, index), 0));
    }
    return result;
}

RuntimeIdentityPtr NetValueCodecImpl::decodeArguments(
    const RuntimeData::Array& arguments) {
    if (arguments.size() > MaxNodes) {
        throw std::invalid_argument("Too many network arguments.");
    }
    const RuntimeHandle packed = ludork::runtime::reference::table();
    ludork::runtime::reference::rawSet(
        packed, "n", static_cast<std::int64_t>(arguments.size()));
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        ludork::runtime::reference::rawSet(packed,
                                           static_cast<std::int64_t>(index + 1),
                                           decodeValue(arguments[index], 0));
    }
    return packed.identity();
}

}  // namespace ludork::global::net_impl
