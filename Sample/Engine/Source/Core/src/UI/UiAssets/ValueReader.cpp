#include "ValueReader.hpp"

#include <Runtime/RuntimeValueReader.hpp>
#include <stdexcept>

namespace ludork::engine::ui_asset_runtime_impl {

sf::Vector2f requireVector2f(RuntimeValueView value,
                             const std::string& source) {
    RuntimeArrayView array =
        ludork::runtime::value_reader::requireArray(value, source);
    if (array.size() != 2) {
        throw std::invalid_argument(source + " must contain two numbers");
    }
    return {
        ludork::runtime::value_reader::requireFloat(array[0], source + "[0]"),
        ludork::runtime::value_reader::requireFloat(array[1], source + "[1]")};
}

}  // namespace ludork::engine::ui_asset_runtime_impl
