#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <vector>

enum class RuntimeLookupMode {
    Visible,
    Own,
};

class LUDORK_RUNTIME_API RuntimeReflectionFacade {
public:
    std::string kind(const RuntimeValue& value) const;
    RuntimeValue typeOf(const RuntimeValue& value) const;
    bool isSubclass(const RuntimeValue& value,
                    const RuntimeValue& targetClass) const;
    bool isInstance(const RuntimeValue& value,
                    const RuntimeValue& targetClass) const;
    bool equal(const RuntimeValue& left, const RuntimeValue& right) const;
    RuntimeValue::Array mro(const RuntimeHandle& classType) const;
    std::vector<std::string> keys(
        const RuntimeHandle& value,
        RuntimeLookupMode mode = RuntimeLookupMode::Visible) const;
    RuntimeValue get(const RuntimeHandle& value, const std::string& name,
                     RuntimeLookupMode mode = RuntimeLookupMode::Visible) const;
    void set(const RuntimeHandle& value, const std::string& name,
             const RuntimeValue& member) const;
    std::string toString(const RuntimeValue& value) const;
    RuntimeValue construct(const RuntimeHandle& classType,
                           const RuntimeValue::Array& arguments = {}) const;
    RuntimeValue::Array call(const RuntimeHandle& receiver,
                             const std::string& name,
                             const RuntimeValue::Array& arguments = {}) const;
    RuntimeValue::Array invoke(const RuntimeHandle& callable,
                               const RuntimeValue::Array& arguments = {}) const;
    RuntimeValue clone(const RuntimeValue& value) const;
};

LUDORK_RUNTIME_API RuntimeReflectionFacade& runtimeReflection();
