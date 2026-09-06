#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/RichText.hpp>

BIND_CLASS()
class FunctionalRichText : public RichText, public FunctionalBase {
public:
    BIND_INIT()
    FunctionalRichText(std::shared_ptr<RichText::RichTextConfig> config,
                       const std::string& text);
    virtual ~FunctionalRichText() = default;
};
