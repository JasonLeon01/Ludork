#pragma once

#include <Input/TextInputHost.hpp>

#include <memory>

namespace ludork::application {

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createAndroidTextInputHost();

}
