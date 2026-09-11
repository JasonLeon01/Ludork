#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Input/InputNamedValue.hpp>
#include <cstdint>

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API JoystickButton {
public:
    enum class Family {
        Unknown,
        Xbox,
        PlayStation4,
        PlayStation5
    };

    BIND_METHOD(metadata = false)
    static InputNamedValue getA();

    BIND_METHOD(metadata = false)
    static InputNamedValue getB();

    BIND_METHOD(metadata = false)
    static InputNamedValue getX();

    BIND_METHOD(metadata = false)
    static InputNamedValue getY();

    BIND_METHOD(metadata = false)
    static InputNamedValue getLB();

    BIND_METHOD(metadata = false)
    static InputNamedValue getRB();

    BIND_METHOD(metadata = false)
    static InputNamedValue getView();

    BIND_METHOD(metadata = false)
    static InputNamedValue getMenu();

    BIND_METHOD(metadata = false)
    static InputNamedValue getLS();

    BIND_METHOD(metadata = false)
    static InputNamedValue getRS();

    BIND_METHOD(metadata = false)
    static InputNamedValue getXBox();

    BIND_METHOD(metadata = false)
    static std::optional<InputNamedValue> getShare();

    static bool isValid(const InputNamedValue& button);
    static std::optional<InputNamedValue> fromName(const std::string& name);
    static std::optional<unsigned int> resolve(unsigned int joystickId,
                                               const InputNamedValue& button);
    static Family getFamily(unsigned int joystickId);
    static Family getDisplayFamily();
    static std::uint64_t getPresentationRevision();

private:
    static std::optional<InputNamedValue> get(const std::string& name);
};
