#include <Input/InputActionKey.hpp>

bool InputActionKey::operator==(const InputActionKey& other) const {
    if (kind != other.kind || name != other.name || code != other.code ||
        threshold != other.threshold) {
        return false;
    }
    if (kind != InputActionKind::JoystickAxis) {
        return true;
    }
    if (comparisonIdentity || other.comparisonIdentity) {
        return comparisonIdentity && other.comparisonIdentity &&
               comparisonIdentity->equals(*other.comparisonIdentity);
    }
    if (!comparison || !other.comparison) {
        return static_cast<bool>(comparison) ==
               static_cast<bool>(other.comparison);
    }
    using FunctionPointer = bool (*)(float, float);
    const FunctionPointer* left = comparison.target<FunctionPointer>();
    const FunctionPointer* right = other.comparison.target<FunctionPointer>();
    return left != nullptr && right != nullptr && *left == *right;
}
