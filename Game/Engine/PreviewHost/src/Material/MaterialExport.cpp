#include "Protocol/PreviewMessageNames.hpp"
#include "MaterialExport.hpp"

#include "Protocol/PreviewProtocol.hpp"

#include <Graphics/Materials/MaterialCompileError.hpp>
#include <Graphics/Materials/MaterialGenerated.hpp>
#include <Graphics/Materials/MaterialResource.hpp>
#include <Runtime/RuntimeDataReader.hpp>

#include <stdexcept>

namespace ludork::preview_host {

RuntimeData MaterialExport::compile(const RuntimeData::Map& request) {
    const std::string& path = ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(request, "materialPath",
                                                    "material export"),
        "materialPath");
    const std::string& layerPath = ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(request, "layerPath",
                                                    "material export"),
        "layerPath");
    try {
        const auto material =
            ludork::engine::materials::MaterialResource::fromData(
                path.empty() ? ludork::engine::materials::MaterialResource::
                                   defaultDocument()
                             : ludork::runtime::value_reader::requireValue(
                                   request, "asset", "material export"),
                path);
        const auto layer =
            layerPath.empty()
                ? nullptr
                : ludork::engine::materials::MaterialResource::fromData(
                      ludork::runtime::value_reader::requireValue(
                          request, "layerAsset", "material export"),
                      layerPath);
        RuntimeData result =
            ludork::engine::materials::MaterialGenerated::compile(*material,
                                                                  layer.get());
        RuntimeData::Map fields = *result.getIf<RuntimeData::Map>();
        fields.emplace("type", RuntimeData(CompiledResponse));
        return RuntimeData(std::move(fields));
    } catch (const ludork::engine::materials::MaterialCompileError& error) {
        return RuntimeData(object({
            {"type", RuntimeData(protocol::response::Error)},
            {"nodeId", RuntimeData(error.nodeId())},
            {"portId", RuntimeData(error.portId())},
            {"message",
             RuntimeData(
                 "Material export failed for " +
                 (path.empty() ? std::string("built-in default") : path) +
                 (layerPath.empty() ? "" : " + " + layerPath) + " (" +
                 error.nodeId() + "." + error.portId() + "): " + error.what())},
        }));
    } catch (const std::exception& error) {
        return errorResponse(
            "Material export failed for " +
            (path.empty() ? std::string("built-in default") : path) +
            (layerPath.empty() ? "" : " + " + layerPath) + ": " + error.what());
    }
}

}  // namespace ludork::preview_host
