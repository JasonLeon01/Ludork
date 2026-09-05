#include <Runtime/NodeGraph/ClassDict.hpp>

#include <Runtime/Blueprint/ClassRuntime.hpp>

RuntimeValue ClassDict::get(const std::optional<std::string>& classPath,
                            const std::optional<std::string>& root) {
    return classPath.has_value()
               ? RuntimeValue(
                     classRuntime().resolve(*classPath, root).classType)
               : RuntimeValue();
}

RuntimeValue ClassDict::getData(const std::string& classPath) {
    return classRuntime().classData(classPath);
}

RuntimeValue ClassDict::instantiateGraph(const std::string& classPath,
                                         RuntimeValue parent) {
    return classRuntime().instantiateGraph(classPath, parent);
}

bool ClassDict::containsCached(const std::string& classPath) const {
    return classRuntime().containsCached(classPath);
}

std::optional<std::string> ClassDict::findCachedPathByName(
    const std::string& className) const {
    return classRuntime().findCachedPathByName(className);
}
