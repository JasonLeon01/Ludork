#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

enum class InputType {
    Mouse = 0,
    Gamepad = 1
};

using InputAxisComparison = std::function<bool(float, float)>;

enum class InputActionKind {
    KeyOrScan,
    Key,
    Scan,
    MouseButton,
    JoystickButton,
    JoystickAxis,
    TouchTap
};

BIND_CLASS(copyable = true, table_init = true, metadata = false,
           lua_alternatives = "integer=>value=$", lua_tostring = "name")
struct InputNamedValue {
    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int value = 0;
};

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API JoystickButton {
public:
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

private:
    static std::optional<InputNamedValue> get(const std::string& name);
};

BIND_MODULE_PROPERTY(name = "InputType", metadata = false, cache = true)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, InputNamedValue>
    inputTypes;

BIND_MODULE_PROPERTY(name = "ActionKind", metadata = false)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, int>
    inputActionKinds;

BIND_MODULE_PROPERTY(name = "AxisComparison", metadata = false)
extern LUDORK_ENGINE_API const
    std::unordered_map<std::string, InputAxisComparison>
        inputAxisComparisons;

BIND_CLASS(
    copyable = true, table_init = true,
    lua_alternatives =
        "number=>kind=InputActionKind::KeyOrScan,code=$;fields(name,value)=>"
        "kind=InputActionKind::JoystickButton,name=$name,code=$value;array("
        "axis,threshold,comparison)=>kind=InputActionKind::JoystickAxis,code=$"
        "axis,threshold=$threshold,comparison=$comparison,comparisonIdentity=$"
        "comparison",
    lua_emit =
        "kind=InputActionKind::KeyOrScan=>value($code);kind=InputActionKind::"
        "Key=>fields(kind=$kind,code=$code);kind=InputActionKind::Scan=>fields("
        "kind=$kind,code=$code);kind=InputActionKind::JoystickButton=>fields("
        "name=$name,value=$code);kind=InputActionKind::JoystickAxis=>array($"
        "code,$threshold,$comparison)")
struct InputActionKey {
    BIND_PROPERTY()
    InputActionKind kind = InputActionKind::KeyOrScan;

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int code = 0;

    BIND_PROPERTY()
    float threshold = 0.0f;

    BIND_PROPERTY(metadata = false)
    InputAxisComparison comparison;

    BIND_PROPERTY(metadata = false)
    RuntimeIdentityPtr comparisonIdentity;

    bool operator==(const InputActionKey& other) const;
};
