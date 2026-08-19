#include "SystemRuntime.hpp"

namespace ludork::global::system_runtime {

SystemRuntime& runtime() {
    static SystemRuntime state;
    return state;
}

}  // namespace ludork::global::system_runtime
