#include "Bindings.hpp"

#include <Base64.hpp>
#include <Compression.hpp>

#include <LuaGlue/LuaGlue.hpp>
#include <zlib.h>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::binding {

void registerCodecs(lua_glue::StateView lua) {
    lua_glue::Table zlib = lua.create_table();
    zlib.set_function("compress", [](const std::string& value) {
        const std::vector<std::uint8_t> bytes = compressZlib(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(value.data()),
                value.size()),
            Z_DEFAULT_COMPRESSION);
        return std::string(bytes.begin(), bytes.end());
    });
    zlib.set_function("decompress", [](const std::string& value) {
        const std::vector<std::uint8_t> bytes = decompressZlib(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(value.data()),
                value.size()),
            15 + 32);
        return std::string(bytes.begin(), bytes.end());
    });
    lua["zlib"] = std::move(zlib);

    lua_glue::Table base64 = lua.create_table();
    base64.set_function("encode", [](const std::string& value) {
        return encodeBase64(std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
    });
    base64.set_function("decode", [](const std::string& value) {
        try {
            const std::vector<std::uint8_t> bytes = decodeBase64(value);
            return std::string(bytes.begin(), bytes.end());
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("Invalid base64 data");
        }
    });
    lua["base64"] = std::move(base64);
}

}  // namespace ludork::standard::binding
