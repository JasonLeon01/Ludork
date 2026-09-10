#pragma once

#include <Runtime/RuntimeData.hpp>
#include <UI/UiControlAdapterDescriptors.hpp>
#include <UI/UiControlPropertyValue.hpp>

namespace ui_control_adapter_detail {

RuntimeData parsePropertyDefault(const UiControlPropertyDescriptor& property,
                                 const std::string& source);

UiControlProperties parseProperties(
    const UiControlAdapterDescriptor& descriptor,
    const RuntimeData::Map& properties, const std::string& source);

}  // namespace ui_control_adapter_detail
