#include <Runtime/RuntimeReference.hpp>
#include "EngineClassRuntimeInternal.hpp"

#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LuaError.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <Runtime/ScriptStore.hpp>
#include <RuntimeSession.hpp>
#include <Utils/DataValue.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::class_runtime_detail {

using namespace ludork::runtime::reference;

std::string normalizeScriptMixinPath(const std::string& value) {
    std::string path = value;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty() || path.front() == '/' ||
        path.find(':') != std::string::npos || !path.ends_with(".lua") ||
        path.ends_with("_meta.lua")) {
        throw std::runtime_error(
            "scriptPath must be a relative .lua path under Scripts/Mixins");
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find('/', start);
        const std::string part = path.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == "." || part == "..") {
            throw std::runtime_error("scriptPath cannot leave Scripts/Mixins");
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return path;
}

bool isScriptMixinReservedName(const std::string& name) {
    return name.starts_with("__") || name == "init" || name == "new" ||
           name == "scriptMixin" || name == "scriptPath" ||
           name == "_GENERATED_CLASS" || name == "_graph" ||
           name == "_hasImplementationOwner";
}

RuntimeValue loadScriptMixin(const std::string& classPath,
                             const std::string& scriptPath) {
    RuntimeValue value;
    try {
        value = first(executeScript("Scripts/Mixins/" + scriptPath));
    } catch (const std::exception& error) {
        throw std::runtime_error("Failed to load Mixin " + scriptPath +
                                 " for " + classPath + ": " + error.what());
    }
    if (!isTable(value)) {
        throw std::runtime_error("Mixin " + scriptPath + " for " + classPath +
                                 " must return a table");
    }
    RuntimeValue mixin = value;
    if (hasMetatable(mixin)) {
        throw std::runtime_error("Mixin " + scriptPath + " for " + classPath +
                                 " must return a table without a metatable");
    }
    for (const auto& entry : entries(mixin)) {
        if (!is<std::string>(entry.first)) {
            throw std::runtime_error("Mixin " + scriptPath + " for " +
                                     classPath +
                                     " must use string member names");
        }
        const std::string name = as<std::string>(entry.first);
        if (isScriptMixinReservedName(name)) {
            throw std::runtime_error("Mixin " + scriptPath + " for " +
                                     classPath + " uses reserved member '" +
                                     name + "'");
        }
    }
    return mixin;
}

void mergeScriptMixin(const RuntimeValue& parentClass,
                      const RuntimeValue& mixin, RuntimeValue definition,
                      RuntimeValue instanceAttrs, const std::string& classPath,
                      const std::string& scriptPath) {
    for (const auto& entry : entries(mixin)) {
        const std::string name = as<std::string>(entry.first);
        const RuntimeValue inherited = get(parentClass, name);
        const bool valueIsFunction = kind(entry.second) == "function";
        const bool inheritedExists = !inherited.isNil();
        if (inheritedExists &&
            (kind(inherited) == "function") != valueIsFunction) {
            throw std::runtime_error(
                "Mixin " + scriptPath + " for " + classPath +
                " changes the member kind of '" + name + "'");
        }
        if (valueIsFunction) {
            rawSet(definition, name, entry.second);
        } else {
            rawSet(definition, name, deepCopy(entry.second));
            rawSet(instanceAttrs, name, deepCopy(entry.second));
        }
    }
}

}  // namespace ludork::engine::class_runtime_detail
