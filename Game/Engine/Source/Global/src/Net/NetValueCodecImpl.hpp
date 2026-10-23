#pragma once

#include <Runtime/RuntimeReference.hpp>

#include <cstddef>
#include <vector>

namespace ludork::global::net_impl {

class NetValueCodecImpl {
public:
    static constexpr std::size_t MaxDepth = 64;
    static constexpr std::size_t MaxNodes = 65536;
    static constexpr std::size_t MaxMessageBytes = 2 * 1024 * 1024;

    RuntimeData encode(const RuntimeIdentityPtr& value);
    RuntimeIdentityPtr decode(const RuntimeData& value);
    RuntimeData::Array encodeArguments(const RuntimeIdentityPtr& arguments);
    RuntimeIdentityPtr decodeArguments(const RuntimeData::Array& arguments);

private:
    RuntimeData encodeValue(const RuntimeValue& value, std::size_t depth,
                            bool key = false);
    RuntimeValue decodeValue(const RuntimeData& value, std::size_t depth,
                             bool key = false);
    void consume(std::size_t depth);
    void consumeBytes(std::size_t bytes);

    std::size_t nodes_ = 0;
    std::size_t bytes_ = 0;
    std::vector<RuntimeIdentityPtr> active_;
};

}  // namespace ludork::global::net_impl
