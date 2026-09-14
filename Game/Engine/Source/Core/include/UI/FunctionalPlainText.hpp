#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/PlainText.hpp>

BIND_CLASS()
class FunctionalPlainText : public PlainText, public FunctionalBase {
public:
    LUDORK_CAST_DERIVED(FunctionalPlainText, PlainText, FunctionalBase)

    BIND_INIT()
    FunctionalPlainText(std::shared_ptr<PlainTextConfig> config,
                        const std::string& text);
    virtual ~FunctionalPlainText() = default;
};
