"""Shared state for the Core binding generator."""

from __future__ import annotations

from dataclasses import dataclass, field

from .model import CallbackCodec


BINDING_FEATURE_HEADERS = {
    "value": "LudorkCoreBinding/ValueCodec.hpp",
    "native": "LudorkCoreBinding/NativeObjectCodec.hpp",
    "dynamic": "LudorkCoreBinding/DynamicValueCodec.hpp",
    "function": "LudorkCoreBinding/FunctionAdapter.hpp",
    "variadic": "LudorkCoreBinding/VariadicFunctionAdapter.hpp",
    "callback": "LudorkCoreBinding/CallbackCodec.hpp",
    "lua_helper": "LudorkCoreBinding/LuaHelper.hpp",
}


BINDING_FEATURE_DEPENDENCIES = {
    "value": (),
    "native": ("value",),
    "dynamic": ("native",),
    "function": ("native",),
    "variadic": ("function",),
    "callback": ("value",),
    "lua_helper": ("value",),
}


@dataclass
class GeneratorContext:
    type_aliases: dict[str, str] = field(default_factory=dict)
    callback_codecs: dict[str, CallbackCodec] = field(default_factory=dict)
    exposed_type_names: dict[str, str] = field(default_factory=dict)
    dynamic_value_types: set[str] = field(default_factory=set)
    table_value_types: set[str] = field(default_factory=set)
    lua_alternative_types: set[str] = field(default_factory=set)
    opaque_identity_types: set[str] = field(default_factory=set)
    suppressed_metadata_base_types: set[str] = field(default_factory=set)
    type_modules: dict[str, str] = field(default_factory=dict)
    binding_features: set[str] = field(default_factory=lambda: {"value"})
    required_bound_types: set[str] = field(default_factory=set)
    required_dynamic_traits: set[str] = field(default_factory=set)
    required_table_traits: set[str] = field(default_factory=set)
    required_opaque_traits: set[str] = field(default_factory=set)

    def fork_translation_unit(self) -> GeneratorContext:
        return GeneratorContext(
            type_aliases=self.type_aliases,
            callback_codecs=self.callback_codecs,
            exposed_type_names=self.exposed_type_names,
            dynamic_value_types=self.dynamic_value_types,
            table_value_types=self.table_value_types,
            lua_alternative_types=self.lua_alternative_types,
            opaque_identity_types=self.opaque_identity_types,
            suppressed_metadata_base_types=self.suppressed_metadata_base_types,
            type_modules=self.type_modules,
        )

    def require_binding_feature(self, feature: str) -> None:
        if feature not in BINDING_FEATURE_HEADERS:
            raise ValueError(f"unknown binding feature: {feature}")
        for dependency in BINDING_FEATURE_DEPENDENCIES[feature]:
            self.require_binding_feature(dependency)
        self.binding_features.add(feature)

    def binding_feature_headers(self) -> list[str]:
        return [
            header
            for feature, header in BINDING_FEATURE_HEADERS.items()
            if feature in self.binding_features
        ]
