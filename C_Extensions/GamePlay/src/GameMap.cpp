#include <GameMap.h>

void ApplyGameMapBinding(py::module &m) {
  ApplyFindPathBinding(m);
  ApplyGetMaterialPropertyMapBinding(m);
  ApplyGetMaterialPropertyTextureBinding(m);
  ApplyRebuildPassabilityCacheBinding(m);
}
