#pragma once

#include <napi/native_api.h>

namespace ludork::application {

bool registerHarmonyTextInputHost(napi_env env, napi_value exports);

}
