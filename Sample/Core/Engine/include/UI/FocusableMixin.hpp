#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <string>
#include <unordered_map>

BIND_MODULE_PROPERTY(name = "FocusDirection", readonly = true)
extern LUDORK_ENGINE_API const std::unordered_map<std::string, std::string>
    FocusDirection;

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API FocusableMixin {
public:
    BIND_INIT()
    FocusableMixin() = default;
    virtual ~FocusableMixin() = default;

    BIND_METHOD()
    void setCanReceiveFocus(bool canReceiveFocus);

    BIND_METHOD(Pure = true)
    bool getCanReceiveFocus() const;

    BIND_METHOD(Pure = true)
    bool getFocused() const;

    BIND_METHOD()
    void setFocused(bool focused);

    BIND_METHOD()
    void setFocusGroup(const std::shared_ptr<RuntimeObject>& focusGroup);

    BIND_METHOD(Pure = true)
    std::shared_ptr<RuntimeObject> getFocusGroup() const;

protected:
    BIND_METHOD()
    virtual void onFocusGained();

    BIND_METHOD()
    virtual void onFocusLost();

private:
    bool canReceiveFocus_ = false;
    bool focused_ = false;
    std::weak_ptr<RuntimeObject> focusGroup_;
};
