"""Shared state for the Core binding generator."""

from __future__ import annotations

from dataclasses import dataclass, field

from .model import CallbackCodec


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
