#pragma once

#include <Runtime/NodeGraph/Node.hpp>
#include <Runtime/NodeGraph/Types.hpp>

#include <string>

namespace ludork::runtime::graph_detail {

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

}  // namespace ludork::runtime::graph_detail
