#include <Color.h>
#include <algorithm>
#include <cctype>
#include <pybind11/stl.h>
#include <stdexcept>


sf::Color C_HexColor(const std::string &value, int alpha) {
  try {
    auto s = value;
    if (s[0] == '#' || s[0] == '$') {
      s = s.substr(1);
    } else if (s.substr(0, 2) == "0x") {
      s = s.substr(2);
    }
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    auto r = static_cast<std::uint8_t>(std::stoi(s.substr(0, 2), nullptr, 16));
    auto g = static_cast<std::uint8_t>(std::stoi(s.substr(2, 2), nullptr, 16));
    auto b = static_cast<std::uint8_t>(std::stoi(s.substr(4, 2), nullptr, 16));
    if (s.size() == 6) {
      return sf::Color(r, g, b, alpha);
    }
    if (s.size() == 8) {
      auto a =
          static_cast<std::uint8_t>(std::stoi(s.substr(6, 2), nullptr, 16));
      return sf::Color(r, g, b, a);
    }
    throw std::runtime_error("Invalid hex color string");
  } catch (const std::exception &e) {
    throw std::runtime_error("Invalid hex color string");
  }
}

sf::Color C_HexColor(int value, int alpha) {
  int r = (value >> 16) & 0xFF;
  int g = (value >> 8) & 0xFF;
  int b = value & 0xFF;
  int a = value >> 24 ? (value >> 24) & 0xFF : alpha;
  return sf::Color(r, g, b, a);
}

void ApplyColorBinding(py::module &m) {
  m.def(
      "C_HexColor",
      [](const std::string &value, int alpha = 255) {
        return C_HexColor(value, alpha);
      },
      py::arg("value"), py::arg("alpha") = 255);
  m.def(
      "C_HexColor",
      [](int value, int alpha = 255) { return C_HexColor(value, alpha); },
      py::arg("value"), py::arg("alpha") = 255);
}
