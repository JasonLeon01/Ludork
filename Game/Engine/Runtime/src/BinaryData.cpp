#include "BinaryData.hpp"

#include <Runtime/Data.hpp>
#include <LudorkGenerated/BinaryDataFormatConstants.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void requireBytes(std::string_view bytes, std::size_t position,
                  std::size_t count) {
    if (position > bytes.size() || count > bytes.size() - position) {
        throw std::runtime_error("Binary data is truncated at byte " +
                                 std::to_string(position));
    }
}

std::uint64_t readUnsigned(std::string_view bytes, std::size_t& position,
                           std::size_t width) {
    requireBytes(bytes, position, width);
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        value |= static_cast<std::uint64_t>(
                     static_cast<unsigned char>(bytes[position++]))
                 << (index * 8);
    }
    return value;
}

std::string readString(std::string_view bytes, std::size_t& position) {
    const std::size_t length = readUnsigned(bytes, position, 4);
    requireBytes(bytes, position, length);
    std::string value(bytes.substr(position, length));
    position += length;
    return value;
}

RuntimeData readValue(std::string_view bytes, std::size_t& position,
                      std::size_t depth) {
    if (depth > ludork::generated::data::MaximumDepth) {
        throw std::runtime_error(
            "Binary data exceeds the maximum nesting depth");
    }
    const std::uint64_t tag = readUnsigned(bytes, position, 1);
    switch (tag) {
        case ludork::generated::data::NullTag:
            return RuntimeData();
        case ludork::generated::data::FalseTag:
            return RuntimeData(false);
        case ludork::generated::data::TrueTag:
            return RuntimeData(true);
        case ludork::generated::data::IntegerTag:
            return RuntimeData(
                std::bit_cast<std::int64_t>(readUnsigned(bytes, position, 8)));
        case ludork::generated::data::FloatTag: {
            const double value =
                std::bit_cast<double>(readUnsigned(bytes, position, 8));
            if (!std::isfinite(value)) {
                throw std::runtime_error("Binary data requires finite numbers");
            }
            return RuntimeData(value);
        }
        case ludork::generated::data::StringTag:
            return RuntimeData(readString(bytes, position));
        case ludork::generated::data::ArrayTag: {
            const std::size_t count = readUnsigned(bytes, position, 4);
            requireBytes(bytes, position, count);
            RuntimeData::Array values;
            for (std::size_t index = 0; index < count; ++index) {
                values.push_back(readValue(bytes, position, depth + 1));
            }
            return RuntimeData(std::move(values));
        }
        case ludork::generated::data::MapTag: {
            const std::size_t count = readUnsigned(bytes, position, 4);
            if (count > (bytes.size() - position) / 5) {
                throw std::runtime_error(
                    "Binary data map count exceeds its payload");
            }
            RuntimeData::Map values;
            for (std::size_t index = 0; index < count; ++index) {
                std::string key = readString(bytes, position);
                RuntimeData item = readValue(bytes, position, depth + 1);
                if (!values.emplace(std::move(key), std::move(item)).second) {
                    throw std::runtime_error(
                        "Binary data contains a duplicate map key");
                }
            }
            return RuntimeData(std::move(values));
        }
        default:
            throw std::runtime_error("Invalid binary data tag at byte " +
                                     std::to_string(position - 1));
    }
}

void checkCapacity(const std::string& output, std::size_t count) {
    if (count > ludork::generated::encrypted::MaximumDataSize - output.size()) {
        throw std::invalid_argument(
            "Binary data exceeds the maximum payload size");
    }
}

void writeUnsigned(std::string& output, std::uint64_t value,
                   std::size_t width) {
    checkCapacity(output, width);
    for (std::size_t index = 0; index < width; ++index) {
        output.push_back(static_cast<char>((value >> (index * 8)) & 0xff));
    }
}

void writeCount(std::string& output, std::size_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("Binary data length exceeds uint32");
    }
    writeUnsigned(output, value, 4);
}

void writeString(std::string& output, const std::string& value) {
    writeCount(output, value.size());
    checkCapacity(output, value.size());
    output.append(value);
}

void writeValue(std::string& output, const RuntimeData& value,
                std::size_t depth) {
    if (depth > ludork::generated::data::MaximumDepth) {
        throw std::invalid_argument(
            "Binary data exceeds the maximum nesting depth");
    }
    if (value.isNil()) {
        writeUnsigned(output, ludork::generated::data::NullTag, 1);
    } else if (const bool* item = value.getIf<bool>()) {
        writeUnsigned(output,
                      *item ? ludork::generated::data::TrueTag
                            : ludork::generated::data::FalseTag,
                      1);
    } else if (const std::int64_t* item = value.getIf<std::int64_t>()) {
        writeUnsigned(output, ludork::generated::data::IntegerTag, 1);
        writeUnsigned(output, std::bit_cast<std::uint64_t>(*item), 8);
    } else if (const double* item = value.getIf<double>()) {
        if (!std::isfinite(*item)) {
            throw std::invalid_argument("Binary data requires finite numbers");
        }
        writeUnsigned(output, ludork::generated::data::FloatTag, 1);
        writeUnsigned(output, std::bit_cast<std::uint64_t>(*item), 8);
    } else if (const std::string* item = value.getIf<std::string>()) {
        writeUnsigned(output, ludork::generated::data::StringTag, 1);
        writeString(output, *item);
    } else if (const RuntimeData::Array* items =
                   value.getIf<RuntimeData::Array>()) {
        writeUnsigned(output, ludork::generated::data::ArrayTag, 1);
        writeCount(output, items->size());
        for (const RuntimeData& item : *items) {
            writeValue(output, item, depth + 1);
        }
    } else if (const RuntimeData::Map* items =
                   value.getIf<RuntimeData::Map>()) {
        writeUnsigned(output, ludork::generated::data::MapTag, 1);
        writeCount(output, items->size());
        std::vector<const RuntimeData::Map::value_type*> ordered;
        ordered.reserve(items->size());
        for (const auto& item : *items) {
            ordered.push_back(&item);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto* left, const auto* right) {
                      return std::lexicographical_compare(
                          left->first.begin(), left->first.end(),
                          right->first.begin(), right->first.end(),
                          [](unsigned char a, unsigned char b) {
                              return a < b;
                          });
                  });
        for (const auto* item : ordered) {
            writeString(output, item->first);
            writeValue(output, item->second, depth + 1);
        }
    }
}

}  // namespace

RuntimeData parseBinaryData(std::string_view bytes) {
    if (bytes.size() > ludork::generated::encrypted::MaximumDataSize) {
        throw std::runtime_error(
            "Binary data exceeds the maximum payload size");
    }
    std::size_t position = 0;
    RuntimeData result = readValue(bytes, position, 0);
    if (position != bytes.size()) {
        throw std::runtime_error("Binary data contains trailing bytes");
    }
    return result;
}

std::string ludork::runtime::encodeBinaryData(const RuntimeData& value) {
    std::string output;
    writeValue(output, value, 0);
    return output;
}
