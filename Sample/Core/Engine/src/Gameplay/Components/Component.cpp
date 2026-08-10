#include <Gameplay/Components/Component.hpp>

RuntimeValue::Array Component::onAttach(const RuntimeIdentityPtr& owner) {
    static_cast<void>(owner);
    return {};
}
