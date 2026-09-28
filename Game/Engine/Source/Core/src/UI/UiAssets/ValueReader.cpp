#include "ValueReader.hpp"

#include <Runtime/RuntimeDataReader.hpp>
#include <stdexcept>

namespace ludork::engine::ui_asset_runtime_impl {

void requireOnlyKeys(const RuntimeData::Map& values,
                     const std::unordered_set<std::string>& allowed,
                     const std::string& source) {
    for (const auto& [name, value] : values) {
        static_cast<void>(value);
        if (!allowed.contains(name)) {
            throw std::invalid_argument(source + " has unknown field " + name);
        }
    }
}

sf::Vector2f requireVector2f(const RuntimeData& value,
                             const std::string& source) {
    const RuntimeData::Array& array =
        ludork::runtime::value_reader::requireArray(value, source);
    if (array.size() != 2) {
        throw std::invalid_argument(source + " must contain two numbers");
    }
    return {
        ludork::runtime::value_reader::requireFloat(array[0], source + "[0]"),
        ludork::runtime::value_reader::requireFloat(array[1], source + "[1]")};
}

}  // namespace ludork::engine::ui_asset_runtime_impl
