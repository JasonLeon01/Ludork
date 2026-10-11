#include <Runtime/Data.hpp>
#include <Runtime/Json.hpp>

#include "BinaryData.hpp"

#include <DataFile.hpp>
#include <LudorkGenerated/ResourceFileConstants.hpp>
#include <StringUtils.hpp>
#include <Utf8Path.hpp>

#include <stdexcept>

RuntimeData loadData(const std::filesystem::path& path) {
    const std::filesystem::path resolved =
        ludork::standard::resolveDataPath(path);
    const std::string bytes = ludork::standard::readFileBytes(resolved);
    if (ludork::standard::lowercase(
            ludork::standard::pathToUtf8(resolved.extension())) ==
        ludork::generated::resources::EncryptedDataExtension) {
        return parseBinaryData(ludork::standard::decodeDataPayload(
            bytes, ludork::standard::pathToUtf8(resolved)));
    }
    return decodeData(bytes);
}

RuntimeData loadData(const std::string& path) {
    return loadData(ludork::standard::pathFromUtf8(path));
}

void saveData(const std::filesystem::path& path, const RuntimeData& value) {
    const std::string extension = ludork::standard::lowercase(
        ludork::standard::pathToUtf8(path.extension()));
    if (extension == ludork::generated::resources::DataExtension) {
        writeJSON(path, value);
    } else if (extension ==
               ludork::generated::resources::EncryptedDataExtension) {
        const std::string payload = ludork::runtime::encodeBinaryData(value);
        ludork::standard::writeFileBytes(
            path, ludork::standard::encodeDataPayload(path, payload));
    } else {
        throw std::invalid_argument("Data file must use .json or .ldc: " +
                                    ludork::standard::pathToUtf8(path));
    }
}

void saveData(const std::string& path, const RuntimeData& value) {
    saveData(ludork::standard::pathFromUtf8(path), value);
}

bool dataExists(const std::filesystem::path& path) {
    return ludork::standard::dataFileExists(path);
}

bool dataExists(const std::string& path) {
    return dataExists(ludork::standard::pathFromUtf8(path));
}

RuntimeData decodeData(const std::string& bytes) {
    if (ludork::standard::isLdcData(bytes)) {
        return parseBinaryData(
            ludork::standard::decodeDataPayload(bytes, "memory"));
    }
    return parseJSONText(bytes);
}
