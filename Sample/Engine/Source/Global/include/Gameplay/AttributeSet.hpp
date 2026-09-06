#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_GLOBAL_API AttributeSet : public RuntimeObject {
public:
    BIND_INIT()
    AttributeSet() = default;

    BIND_METHOD(metadata = false)
    void initialize(const RuntimeValue::Map& values);

    BIND_METHOD(Pure = true)
    std::vector<std::string> getAttributeNames() const;

    BIND_METHOD(Pure = true)
    RuntimeValue getAttributeSchema(const std::string& name) const;

    RuntimeValue getAttributeValue(const std::string& name) const;
    void setAttributeValue(const std::string& name, const RuntimeValue& value);
    std::string getAttributeType(const std::string& name) const;

private:
    RuntimeValue selfValue() const;

    std::vector<std::string> attributeNames_;
    RuntimeValue::Map schema_;
};
