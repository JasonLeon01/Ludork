#include <UI/FunctionalPlainText.hpp>
#include <UI/PlainText.hpp>
#include <UI/PlainTextConfig.hpp>

FunctionalPlainText::FunctionalPlainText(
    std::shared_ptr<PlainTextConfig> config, const std::string& text)
    : PlainText(std::move(config), text), FunctionalBase() {}
