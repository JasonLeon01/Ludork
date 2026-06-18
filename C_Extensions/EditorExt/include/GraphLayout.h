#pragma once

#include <pybind11/pybind11.h>

namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Compute organised node positions for a blueprint graph
///
/// - \param nodeCount Number of persisted graph nodes
/// - \param links Link dictionaries for the graph key
/// - \param nodeRely Parameter dependency map for the graph key
/// - \param startIdx Entry node index, or ``None`` when unset
/// - \param defaultParamCount Number of visual default parameter nodes
/// - \param xStep Horizontal spacing between exec columns
/// - \param yStep Vertical spacing between exec branches
/// - \param defaultParamYStep Vertical spacing between default parameter nodes
/// - \param defaultParamStartY Starting Y for the first default parameter node
/// - \param startGap Gap below default parameter nodes before exec layout
///
/// - \return Mapping of node index or ``default_N`` key to ``(x, y)`` positions
///
////////////////////////////////////////////////////////////
py::dict C_ComputeGraphLayoutPositions(int nodeCount, py::list links, py::dict nodeRely,
                                       py::object startIdx, int defaultParamCount = 0,
                                       double xStep = 720.0, double yStep = 250.0,
                                       double defaultParamYStep = 64.0,
                                       double defaultParamStartY = 64.0, double startGap = 250.0);
