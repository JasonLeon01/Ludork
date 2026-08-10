#include <NodeGraph/ClassDict.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

ClassDict::ClassDict() {
    classes_.emplace("", emptyMap());
}

RuntimeValue ClassDict::get(const std::optional<std::string>& classPath,
                            const std::optional<std::string>& root) {
    if (!classPath.has_value()) {
        return {};
    }

    const auto cached = classes_.find(*classPath);
    if (cached != classes_.end()) {
        return cached->second;
    }

    std::vector<RuntimeValue> arguments;
    arguments.emplace_back(*classPath);
    arguments.emplace_back(root.has_value() ? RuntimeValue(*root)
                                            : RuntimeValue());
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("nodegraph.resolveClass", arguments);
    if (resolved.empty() || resolved.front().isNil()) {
        throw std::runtime_error("Class " + *classPath + " not found");
    }

    classes_.emplace(*classPath, resolved.front());
    const bool dataBacked = resolved.size() > 1 && !resolved[1].isNil();
    if (dataBacked) {
        classData_[*classPath] = resolved[1];
    }
    indexClassPath(*classPath, dataBacked);
    return resolved.front();
}

RuntimeValue ClassDict::getData(const std::string& classPath) {
    const auto cached = classData_.find(classPath);
    if (cached != classData_.end()) {
        return cached->second;
    }

    const std::vector<RuntimeValue> resolved =
        resolveRuntime("nodegraph.classData", {RuntimeValue(classPath)});
    if (resolved.empty() || resolved.front().isNil()) {
        return emptyMap();
    }
    classData_[classPath] = resolved.front();
    return resolved.front();
}

RuntimeValue ClassDict::instantiateGraph(const std::string& classPath,
                                         RuntimeValue parent) {
    if (classes_.find(classPath) == classes_.end()) {
        get(classPath);
    }
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("nodegraph.instantiateClassGraph",
                       {RuntimeValue(classPath), std::move(parent)});
    return resolved.empty() ? RuntimeValue() : resolved.front();
}

void ClassDict::invalidate(const std::string& classPath) {
    unindexClassPath(classPath);
    classes_.erase(classPath);
    classData_.erase(classPath);
    resolveRuntime("nodegraph.invalidateClass", {RuntimeValue(classPath)});
}

bool ClassDict::containsCached(const std::string& classPath) const {
    return classes_.find(classPath) != classes_.end();
}

std::optional<std::string> ClassDict::findCachedPathByName(
    const std::string& className) const {
    const auto cached = classNameIndex_.find(className);
    return cached == classNameIndex_.end()
               ? std::nullopt
               : std::optional<std::string>(cached->second);
}

RuntimeValue ClassDict::emptyMap() {
    return RuntimeValue(RuntimeValue::Map{});
}

void ClassDict::indexClassPath(const std::string& classPath, bool dataBacked) {
    if (classPath.empty()) {
        return;
    }
    const std::size_t separator = classPath.find_last_of('.');
    const std::string leaf = separator == std::string::npos
                                 ? classPath
                                 : classPath.substr(separator + 1);
    if (!dataBacked) {
        classNameIndex_[leaf] = classPath;
        return;
    }
    std::string generatedName = classPath;
    for (char& character : generatedName) {
        if (character == '.') {
            character = '_';
        }
    }
    classNameIndex_[generatedName] = classPath;
}

void ClassDict::unindexClassPath(const std::string& classPath) {
    for (auto entry = classNameIndex_.begin();
         entry != classNameIndex_.end();) {
        if (entry->second == classPath) {
            entry = classNameIndex_.erase(entry);
        } else {
            ++entry;
        }
    }
}
