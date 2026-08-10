#pragma once

#include <BindAnnotations.hpp>
#include <Gameplay/BPBase.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <vector>

BIND_CLASS(bind_bases = false,
           runtime_bases = "BPBase", native_bases = "RuntimeObject",
           cast_bases = "RuntimeObject")
class InfoBase : public RuntimeObject {
public:
    BIND_INIT()
    InfoBase() = default;

    ~InfoBase() override = default;

    BIND_PROPERTY()
    std::string ID = "";

    BIND_METHOD()
    void initInfo(const RuntimeIdentityPtr& dataProvider);

    BIND_METHOD()
    void setInfoGraph(const RuntimeIdentityPtr& graph);

    BIND_METHOD()
    RuntimeIdentityPtr getInfoGraph() const;

    BIND_METHOD()
    bool hasInfoGraph() const;

    BIND_METHOD(defaults = {nil, nil})
    void triggerEvent(const std::string& eventName,
                      const RuntimeValue& keywordArguments = {},
                      const RuntimeIdentityPtr& onComplete = nullptr);

    BIND_METHOD(defaults = {nil})
    static std::vector<std::string> getRegisteredEvents(
        const RuntimeIdentityPtr& classType = nullptr);

    BIND_METHOD()
    static std::string getInfoType(const RuntimeIdentityPtr& classType);

private:
    RuntimeIdentityPtr infoGraph_;
};
