#pragma once

#include <BindAnnotations.hpp>

BIND_FUNCTION_GROUP(name = "NodeGraph")

BIND_FUNCTION(metadata = false)
void initLatent();
