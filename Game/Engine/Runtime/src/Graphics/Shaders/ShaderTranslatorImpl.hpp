#pragma once

#include <cstddef>
#include <string_view>

namespace ludork::runtime::graphics::shader_translator_impl {

struct Token {
    std::size_t begin;
    std::size_t end;
    std::string_view text;
};

class Process final {
public:
    Process();
    ~Process();
};

}  // namespace ludork::runtime::graphics::shader_translator_impl
