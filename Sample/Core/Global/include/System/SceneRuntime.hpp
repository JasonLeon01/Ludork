#pragma once

#include <Runtime/RuntimeValue.hpp>

class SceneRuntime : public RuntimeObject {
public:
    virtual ~SceneRuntime() = default;
    virtual void systemMain() = 0;
    virtual void systemEnter() = 0;
    virtual void systemQuit() = 0;
    virtual void systemDestroy() = 0;
    virtual void systemShutdown() noexcept = 0;
    virtual bool systemIsRunning() const noexcept = 0;
    virtual void systemInput() = 0;
};
