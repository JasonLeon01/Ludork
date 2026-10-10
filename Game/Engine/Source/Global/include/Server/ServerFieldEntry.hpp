#pragma once
#include <GlobalRuntimeApi.hpp>
#include <Runtime/RuntimeData.hpp>
#include <LudorkRuntimeBinding/Annotations.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerFieldEntry {
public:
    ServerFieldEntry(std::string key, RuntimeData value);
    BIND_METHOD(property = "key")
    std::string getKey() const;
    BIND_METHOD(property = "value")
    RuntimeData getValue() const;

private:
    std::string key_;
    RuntimeData value_;
};
