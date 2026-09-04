#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <optional>
#include <string>
#include <unordered_map>

BIND_CLASS(metadata = false)
class ClassDict {
public:
    BIND_INIT()
    ClassDict();

    BIND_METHOD(indexer = true, metadata = false)
    RuntimeValue get(const std::optional<std::string>& classPath,
                     const std::optional<std::string>& root = std::nullopt);

    BIND_METHOD(metadata = false)
    RuntimeValue getData(const std::string& classPath);

    BIND_METHOD(metadata = false, allow_nil = "parent")
    RuntimeValue instantiateGraph(const std::string& classPath,
                                  RuntimeValue parent);

    BIND_METHOD(metadata = false)
    void invalidate(const std::string& classPath);

    BIND_METHOD(metadata = false)
    bool containsCached(const std::string& classPath) const;

    BIND_METHOD(metadata = false)
    std::optional<std::string> findCachedPathByName(
        const std::string& className) const;

private:
    static RuntimeValue emptyMap();
    void indexClassPath(const std::string& classPath, bool dataBacked);
    void unindexClassPath(const std::string& classPath);

    std::unordered_map<std::string, RuntimeValue> classes_;
    std::unordered_map<std::string, RuntimeValue> classData_;
    std::unordered_map<std::string, std::string> classNameIndex_;
};
