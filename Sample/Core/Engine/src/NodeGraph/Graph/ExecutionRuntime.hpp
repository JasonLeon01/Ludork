#pragma once

#include <NodeGraph/Node.hpp>
#include <NodeGraph/Types.hpp>

#include <string>

namespace ludork::engine::graph_detail {

class StringRestore {
public:
    explicit StringRestore(std::string& value);
    ~StringRestore();

    StringRestore(const StringRestore&) = delete;
    StringRestore& operator=(const StringRestore&) = delete;

private:
    std::string& value_;
    std::string previous_;
};

bool runtimeValueEqual(const RuntimeValue& left, const RuntimeValue& right);
RuntimeValue normaliseMatchValue(const RuntimeValue& value);

}  // namespace ludork::engine::graph_detail
