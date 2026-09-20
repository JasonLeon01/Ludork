#pragma once

#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Gameplay/GameplayNumber.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_GLOBAL_API AttributeSet : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(AttributeSet, RuntimeObject)

    BIND_CLASS(copyable = true, table_init = true)
    struct LUDORK_GLOBAL_API AttributeSchema {
        BIND_PROPERTY()
        RuntimeData type;

        BIND_METHOD(property = "default", setter = "setDefault")
        RuntimeValue getDefault() const;
        void setDefault(RuntimeValue value);

    private:
        RuntimeValue defaultValue_;
    };

    enum class NumericType {
        Integer,
        Float
    };

    BIND_INIT()
    AttributeSet() = default;

    BIND_METHOD(metadata = false)
    void initialize(const RuntimeValue::Map& values);

    BIND_METHOD(metadata = false)
    void initializeStored(const RuntimeValue::Map& values);

    BIND_METHOD(Pure = true)
    std::vector<std::string> getAttributeNames() const;

    BIND_METHOD(Pure = true)
    std::optional<AttributeSchema> getAttributeSchema(
        const std::string& name) const;

    GameplayNumber getNumericAttributeValue(const std::string& name) const;
    void setNumericAttributeValue(const std::string& name,
                                  const GameplayNumber& value);
    std::optional<NumericType> getNumericAttributeType(
        const std::string& name) const;

private:
    RuntimeValue selfValue() const;
    void initializeValues(const RuntimeValue::Map& values, bool stored);

    std::vector<std::string> attributeNames_;
    std::unordered_map<std::string, AttributeSchema> schema_;
};
