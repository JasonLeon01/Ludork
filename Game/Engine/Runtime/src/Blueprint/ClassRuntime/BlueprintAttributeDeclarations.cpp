#include "BlueprintAttributeDeclarations.hpp"

#include "ClassRuntimeInternal.hpp"
#include "Metadata/EnumSchema.hpp"
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/TypedDataService.hpp>
#include <StringUtils.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ludork::runtime::class_runtime_detail {

namespace {

[[noreturn]] void invalid(const std::string& classPath,
                          const std::string& message) {
    throw std::invalid_argument("Invalid Blueprint '" + classPath +
                                "': " + message);
}

void requireObject(const RuntimeValue& value, const std::string& classPath,
                   const std::string& name) {
    const RuntimeValue snapshot = reference::snapshot(value);
    if (!reference::isTable(value) || snapshot.view().array().has_value()) {
        invalid(classPath, name + " must be an object");
    }
    for (const auto& [key, item] :
         reference::entries(reference::intern(value))) {
        if (!reference::is<std::string>(key)) {
            invalid(classPath, name + " must use string keys");
        }
    }
}

bool identifier(const std::string& name) {
    return !name.empty() && ludork::standard::trimCharacters(name) == name &&
           (name.front() < '0' || name.front() > '9');
}

void validateExtraType(const TypeSchema& type) {
    if (type.kind == TypeSchema::Kind::Optional) {
        throw std::invalid_argument(
            "optional is not an attribute declaration schema");
    }
    if (!type.arguments.empty()) {
        for (const TypeSchema& argument : type.arguments) {
            validateExtraType(argument);
        }
        return;
    }
    if (type.kind == TypeSchema::Kind::Enum) {
        const RuntimeValue typeSchema(
            RuntimeValue::Map{{"enum", RuntimeValue(type.name)}});
        const RuntimeValue constants = reference::snapshot(
            typedDataService().resolveMetadataType(typeSchema));
        static_cast<void>(detail::enumValueType(constants.view(), type.name));
        return;
    }
    const std::string name =
        type.module.empty() ? type.name : type.module + "." + type.name;
    const RuntimeValue namedType(name);
    if (name != "function" && name != "event" &&
        !typedDataService().isStandardValueType(namedType.view()) &&
        typedDataService().resolveMetadataType(namedType).isNil()) {
        throw std::invalid_argument("unknown extra attribute type '" + name +
                                    "'");
    }
}

void validateSchemaValue(RuntimeValueView value, const std::string& classPath,
                         const std::string& name) {
    if (value.isNil()) {
        invalid(classPath, "null type schema in " + name);
    }
    if (const auto array = value.array()) {
        for (RuntimeValueView item : *array) {
            validateSchemaValue(item, classPath, name);
        }
    } else if (const auto map = value.map()) {
        for (const auto& [key, item] : *map) {
            validateSchemaValue(item, classPath, name);
        }
    }
}

}  // namespace

BlueprintAttributeDeclarations resolveBlueprintAttributeDeclarations(
    const RuntimeValue& parentClass, const RuntimeValue& rawDeclarations,
    const RuntimeValue& mixin, const std::string& scriptPath,
    const std::string& classPath) {
    BlueprintAttributeDeclarations result{
        reference::intern(reference::deepCopy(
            typedDataService().getAttrMetadata(parentClass))),
        reference::table(), reference::table()};
    const RuntimeHandle localNames = reference::table();
    reference::rawSet(result.localMetadata, "attrs", localNames);
    std::size_t localIndex = 1;
    std::string module;
    if (!scriptPath.empty()) {
        module = "Mixins." + scriptPath.substr(0, scriptPath.size() - 4);
        std::replace(module.begin(), module.end(), '/', '.');
    }
    if (!module.empty() && reference::moduleExists(module + "_meta")) {
        const RuntimeHandle root = requireModuleTable(module + "_meta");
        const auto rootEntries = reference::entries(root);
        const std::string typeName =
            module.substr(module.find_last_of('.') + 1);
        if (rootEntries.size() != 1 ||
            !reference::is<std::string>(rootEntries.front().first) ||
            reference::as<std::string>(rootEntries.front().first) != typeName) {
            invalid(classPath,
                    "Mixin metadata must contain only type '" + typeName + "'");
        }
        const RuntimeValue metadata = rootEntries.front().second;
        requireObject(metadata, classPath, "Mixin metadata");
        static_cast<void>(reference::snapshot(metadata).toData());
        const RuntimeValue bases =
            reference::rawGet(reference::intern(metadata), "bases");
        if (!bases.isNil() &&
            (!reference::isTable(bases) ||
             !reference::entries(reference::intern(bases)).empty())) {
            invalid(classPath, "Mixin metadata cannot declare bases");
        }
        const RuntimeValue names =
            reference::rawGet(reference::intern(metadata), "attrs");
        const auto array = reference::arrayValues(names);
        if (!reference::isTable(names) || !array) {
            invalid(classPath, "Mixin metadata attrs must be an ordered array");
        }
        std::unordered_set<std::string> declared;
        for (const RuntimeValue& item : *array) {
            if (!reference::is<std::string>(item)) {
                invalid(classPath, "Mixin metadata attrs must contain names");
            }
            const std::string name = reference::as<std::string>(item);
            if (name.empty() || isScriptMixinReservedName(name) ||
                !declared.insert(name).second) {
                invalid(classPath,
                        "invalid or repeated Mixin attribute '" + name + "'");
            }
            if (reference::kind(reference::rawGet(reference::intern(mixin),
                                                  name)) == "function") {
                invalid(classPath, "Mixin attribute is a method: " + name);
            }
            const RuntimeValue field =
                reference::rawGet(reference::intern(metadata), name);
            requireObject(field, classPath, "Mixin attribute '" + name + "'");
            const RuntimeValue type = reference::snapshot(
                reference::rawGet(reference::intern(field), "type"));
            if (type.isNil()) {
                invalid(classPath, "Mixin attribute has no type: " + name);
            }
            validateSchemaValue(type.view(), classPath,
                                "Mixin attribute " + name);
            try {
                static_cast<void>(typedDataService().compileType(type.view()));
            } catch (const std::exception& error) {
                invalid(classPath,
                        "Mixin attrs." + name + ".type: " + error.what());
            }
            for (const RuntimeValue& ancestor :
                 reference::classMro(reference::intern(parentClass))) {
                if (reference::rawGet(reference::intern(ancestor),
                                      "__blueprintClassPath")
                        .isNil()) {
                    continue;
                }
                const RuntimeValue declaredTypes =
                    reference::rawGet(reference::intern(ancestor), "__types");
                if (!reference::isTable(declaredTypes)) {
                    continue;
                }
                const RuntimeValue existing = reference::snapshot(
                    reference::rawGet(reference::intern(declaredTypes), name));
                if (!existing.isNil()) {
                    invalid(
                        classPath,
                        "Mixin metadata conflicts with inherited attrDefs: " +
                            name);
                }
            }
            const RuntimeHandle member =
                reference::intern(reference::deepCopy(field));
            reference::rawSet(member, "module", module);
            reference::rawSet(localNames, localIndex++, name);
            reference::rawSet(result.localMetadata, name, member);
            const RuntimeHandle descriptor = reference::table();
            reference::rawSet(descriptor, "type", type);
            reference::rawSet(descriptor, "module", module);
            reference::rawSet(descriptor, "component",
                              reference::rawGet(member, "component"));
            reference::rawSet(descriptor, "metadata", member);
            reference::rawSet(result.metadata, name, descriptor);
        }
    }
    if (rawDeclarations.isNil()) {
        return result;
    }
    requireObject(rawDeclarations, classPath, "attrDefs");
    for (const auto& [rawName, declaration] :
         reference::entries(reference::intern(rawDeclarations))) {
        const std::string name = reference::as<std::string>(rawName);
        if (!identifier(name) || isScriptMixinReservedName(name)) {
            invalid(classPath,
                    "invalid or reserved attribute name '" + name + "'");
        }
        if (!reference::rawGet(result.metadata, name).isNil() ||
            !reference::get(reference::intern(parentClass), name).isNil() ||
            (!mixin.isNil() &&
             !reference::rawGet(reference::intern(mixin), name).isNil())) {
            invalid(
                classPath,
                "attribute declaration conflicts with an existing member: " +
                    name);
        }
        requireObject(declaration, classPath, "attrDefs." + name);
        for (const auto& [key, value] :
             reference::entries(reference::intern(declaration))) {
            const std::string field = reference::as<std::string>(key);
            if (field != "type" && field != "base") {
                invalid(classPath, "unsupported declaration field attrDefs." +
                                       name + "." + field);
            }
        }
        const RuntimeValue type = reference::snapshot(
            reference::rawGet(reference::intern(declaration), "type"));
        if (type.isNil()) {
            invalid(classPath, "attribute declaration has no type: " + name);
        }
        validateSchemaValue(type.view(), classPath, "attrDefs." + name);
        TypeSchema schema;
        try {
            schema = typedDataService().compileType(type.view());
            validateExtraType(schema);
        } catch (const std::exception& error) {
            invalid(classPath, "attrDefs." + name + ".type: " + error.what());
        }
        const RuntimeValue base =
            reference::rawGet(reference::intern(declaration), "base");
        if (!base.isNil() &&
            (schema.kind != TypeSchema::Kind::Named || !schema.module.empty() ||
             schema.name != "file" || !reference::is<std::string>(base))) {
            invalid(classPath, "attrDefs." + name +
                                   ".base is only valid for file attributes");
        }
        reference::rawSet(result.localTypes, name, reference::deepCopy(type));
        const RuntimeHandle descriptor = reference::table();
        reference::rawSet(descriptor, "type", reference::deepCopy(type));
        reference::rawSet(descriptor, "component", false);
        reference::rawSet(result.metadata, name, descriptor);
    }
    return result;
}

void validateBlueprintAttributes(const RuntimeValue& attributes,
                                 const RuntimeHandle& metadata,
                                 const std::string& classPath) {
    requireObject(attributes, classPath, "attrs");
    for (const auto& [key, value] :
         reference::entries(reference::intern(attributes))) {
        const std::string name = reference::as<std::string>(key);
        if (!reference::isTable(reference::rawGet(metadata, name))) {
            invalid(classPath, "undeclared attribute '" + name + "'");
        }
    }
}

}  // namespace ludork::runtime::class_runtime_detail
