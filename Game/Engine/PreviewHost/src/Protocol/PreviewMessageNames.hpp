#pragma once

namespace ludork::preview_host::protocol {

namespace response {
inline constexpr char Error[] = "error";
}

namespace command {
inline constexpr char Close[] = "close";
inline constexpr char Load[] = "load";
inline constexpr char Render[] = "render";
inline constexpr char Seek[] = "seek";
}  // namespace command

}  // namespace ludork::preview_host::protocol
