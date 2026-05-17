#pragma once

#include <BindAnnotations.hpp>

#include <pybind11/pybind11.h>

#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>


namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Compress timeline animation frames into binary payloads
///
/// The return tuple contains:
/// - animation duration in seconds
/// - compressed frame images
/// - sound trigger entries
///
/// - \param zlibModule Python `zlib` module-like object
/// - \param frameCount Number of frames to render
/// - \param frameStep Time step per frame in seconds
/// - \param frameRate Frame rate used for timeline conversion
/// - \param timeLines Timeline track definitions
/// - \param assets Asset file names
/// - \param assetsRoot Root directory of `assets`
/// - \param imageFormat Export image format for each frame
///
/// - \return Tuple of duration, compressed frames and sound entries
///
////////////////////////////////////////////////////////////
BIND_FUNCTION()
std::tuple<float, std::vector<py::bytes>, std::vector<py::dict>> C_CompressAnimation(
    py::object zlibModule, int frameCount, float frameStep, int frameRate,
    const std::vector<std::unordered_map<std::string, std::vector<py::dict>>> &timeLines,
    const std::vector<std::string> &assets, const std::string &assetsRoot, const std::string &imageFormat);
