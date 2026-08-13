from __future__ import annotations

import json
import re
from pathlib import Path

from .context import GeneratorContext
from .cpp_types import normalize_declaration, parse_cpp_type
from .model import CallbackCodec


_DIRECTIONS = {"fromLua", "toLua"}
_THREAD_POLICIES = {"blockingEnter", "nativeTryEnter", "nativeThreadBoundary"}
_ACCESS_MODES = {"read", "write", "readWrite"}


def _required_string(value: object, path: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{path} must be a non-empty string")
    return value


def _optional_string(value: object, path: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{path} must be a string")
    return value


def _validate_parameters(value: object, path: str) -> None:
    if not isinstance(value, list):
        raise ValueError(f"{path} must be an array")
    names: set[str] = set()
    for index, raw_parameter in enumerate(value):
        prefix = f"{path}[{index}]"
        if not isinstance(raw_parameter, dict):
            raise ValueError(f"{prefix} must be an object")
        expected_keys = {"name", "role", "access", "nullable", "unit"}
        if set(raw_parameter) != expected_keys:
            raise ValueError(f"{prefix} must contain {sorted(expected_keys)}")
        name = _required_string(raw_parameter.get("name"), f"{prefix}.name")
        if name in names:
            raise ValueError(f"{path} contains duplicate parameter {name}")
        names.add(name)
        _required_string(raw_parameter.get("role"), f"{prefix}.role")
        access = _required_string(raw_parameter.get("access"), f"{prefix}.access")
        if access not in _ACCESS_MODES:
            raise ValueError(f"{prefix}.access is unsupported: {access}")
        if not isinstance(raw_parameter.get("nullable"), bool):
            raise ValueError(f"{prefix}.nullable must be boolean")
        _optional_string(raw_parameter.get("unit"), f"{prefix}.unit")


def _validate_returns(value: object, path: str) -> None:
    if not isinstance(value, list):
        raise ValueError(f"{path} must be an array")
    names: set[str] = set()
    for index, raw_return in enumerate(value):
        prefix = f"{path}[{index}]"
        if not isinstance(raw_return, dict):
            raise ValueError(f"{prefix} must be an object")
        expected_keys = {"name", "role", "nullable", "unit"}
        if set(raw_return) != expected_keys:
            raise ValueError(f"{prefix} must contain {sorted(expected_keys)}")
        name = _required_string(raw_return.get("name"), f"{prefix}.name")
        if name in names:
            raise ValueError(f"{path} contains duplicate return {name}")
        names.add(name)
        _required_string(raw_return.get("role"), f"{prefix}.role")
        if not isinstance(raw_return.get("nullable"), bool):
            raise ValueError(f"{prefix}.nullable must be boolean")
        _optional_string(raw_return.get("unit"), f"{prefix}.unit")


def _validate_canonical_type(value: object, path: str) -> str:
    canonical_type = normalize_declaration(_required_string(value, path))
    parsed = parse_cpp_type_for_manifest(canonical_type)
    if parsed.name != "std::function" or len(parsed.arguments) != 1:
        raise ValueError(f"{path} must be a canonical std::function type")
    return canonical_type


def parse_cpp_type_for_manifest(value: str):
    return parse_cpp_type(GeneratorContext(), value)


def _canonical_key(value: str) -> str:
    result = re.sub(r"\s+", "", normalize_declaration(value))
    substitutions = {
        "std::uint32_t": "unsignedint",
        "uint32_t": "unsignedint",
        "std::int32_t": "int",
        "int32_t": "int",
    }
    for source, target in substitutions.items():
        result = result.replace(source, target)
    return result


def _load_sfml_callback_aliases(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise ValueError(f"LuaSF API schema does not exist: {path}")
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"failed to read LuaSF API schema {path}: {error}") from error
    result: dict[str, str] = {}

    def visit(value: object) -> None:
        if isinstance(value, list):
            for item in value:
                visit(item)
            return
        if not isinstance(value, dict):
            return
        if value.get("kind") == "TYPE_ALIAS_DECL":
            qualified_name = value.get("qualified_name")
            raw_type = value.get("type")
            if isinstance(qualified_name, str) and isinstance(raw_type, dict):
                raw_canonical = raw_type.get("canonical")
                if isinstance(raw_canonical, str) and raw_canonical:
                    canonical = normalize_declaration(raw_canonical)
                    previous = result.get(qualified_name)
                    if previous is not None and _canonical_key(
                        previous
                    ) != _canonical_key(canonical):
                        raise ValueError(
                            f"LuaSF API schema has conflicting aliases for "
                            f"{qualified_name}: {previous} != {canonical}"
                        )
                    result[qualified_name] = canonical
        for item in value.values():
            visit(item)

    visit(document)
    return result


def validate_callback_codec_aliases(
    context: GeneratorContext, sfml_api_path: Path
) -> None:
    sfml_aliases = _load_sfml_callback_aliases(sfml_api_path)
    for cpp_name, codec in context.callback_codecs.items():
        sfml_declared = sfml_aliases.get(cpp_name)
        if sfml_declared is None:
            raise ValueError(
                f"callback codec alias {cpp_name} is missing from {sfml_api_path}"
            )
        if _canonical_key(sfml_declared) != _canonical_key(codec.canonical_type):
            raise ValueError(
                f"callback codec alias {cpp_name} canonical type mismatch: "
                f"{sfml_declared} != {codec.canonical_type}"
            )
        declared = context.type_aliases.get(cpp_name)
        if declared is not None and _canonical_key(declared) != _canonical_key(
            codec.canonical_type
        ):
            raise ValueError(
                f"callback codec alias {cpp_name} canonical type mismatch: "
                f"{declared} != {codec.canonical_type}"
            )


def load_callback_codecs(path: Path) -> dict[str, CallbackCodec]:
    if not path.is_file():
        raise ValueError(f"callback codec manifest does not exist: {path}")
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(
            f"failed to read callback codec manifest {path}: {error}"
        ) from error
    if not isinstance(document, dict):
        raise ValueError("callback codec manifest root must be an object")
    if set(document) != {"schemaVersion", "callbacks"}:
        raise ValueError(
            "callback codec manifest must contain schemaVersion and callbacks"
        )
    if document.get("schemaVersion") != 1:
        raise ValueError("callback codec manifest schemaVersion must be 1")
    callbacks = document.get("callbacks")
    if not isinstance(callbacks, list):
        raise ValueError("callback codec manifest callbacks must be an array")
    result: dict[str, CallbackCodec] = {}
    selectors: set[tuple[str, ...]] = set()
    for index, raw_callback in enumerate(callbacks):
        prefix = f"callback codec manifest callbacks[{index}]"
        if not isinstance(raw_callback, dict):
            raise ValueError(f"{prefix} must be an object")
        required_keys = {
            "name",
            "selector",
            "canonicalType",
            "codec",
            "luaType",
            "luaSignature",
            "allowNil",
            "threadPolicy",
            "directions",
            "parameters",
            "returns",
            "clearSetterOnQuiesce",
        }
        optional_keys = {"nativeType"}
        actual_keys = set(raw_callback)
        if not required_keys.issubset(actual_keys) or not actual_keys.issubset(
            required_keys | optional_keys
        ):
            raise ValueError(f"{prefix} has an invalid field set")
        selector = raw_callback.get("selector")
        if not isinstance(selector, dict):
            raise ValueError(f"{prefix}.selector must be an object")
        selector_kind = selector.get("kind")
        cpp_name = ""
        if selector_kind == "alias":
            if set(selector) != {"kind", "cppName"}:
                raise ValueError(
                    f"{prefix}.selector alias must contain kind and cppName"
                )
            cpp_name = _required_string(
                selector.get("cppName"), f"{prefix}.selector.cppName"
            )
            selector_key = ("alias", cpp_name)
        elif selector_kind == "functionParameter":
            required_selector_keys = {
                "kind",
                "qualifiedFunction",
                "parameterName",
                "callableSignature",
            }
            selector_keys = set(selector)
            if not required_selector_keys.issubset(
                selector_keys
            ) or not selector_keys.issubset(required_selector_keys | {"cppName"}):
                raise ValueError(
                    f"{prefix}.selector functionParameter must contain "
                    f"{sorted(required_selector_keys)} and optional cppName"
                )
            if "cppName" in selector:
                _required_string(selector.get("cppName"), f"{prefix}.selector.cppName")
            selector_key = (
                "functionParameter",
                _required_string(
                    selector.get("qualifiedFunction"),
                    f"{prefix}.selector.qualifiedFunction",
                ),
                _required_string(
                    selector.get("parameterName"),
                    f"{prefix}.selector.parameterName",
                ),
                _required_string(
                    selector.get("callableSignature"),
                    f"{prefix}.selector.callableSignature",
                ),
            )
        else:
            raise ValueError(f"{prefix}.selector.kind is unsupported")
        if selector_key in selectors:
            raise ValueError(f"duplicate callback codec selector: {selector_key}")
        selectors.add(selector_key)

        canonical_type = _validate_canonical_type(
            raw_callback.get("canonicalType"), f"{prefix}.canonicalType"
        )
        if selector_kind == "functionParameter":
            expected_canonical_type = f"std::function<{selector_key[3]}>"
            if _canonical_key(canonical_type) != _canonical_key(
                expected_canonical_type
            ):
                raise ValueError(
                    f"{prefix}.canonicalType does not match callableSignature"
                )
        codec = _required_string(raw_callback.get("codec"), f"{prefix}.codec")
        lua_type = _required_string(raw_callback.get("luaType"), f"{prefix}.luaType")
        _required_string(raw_callback.get("luaSignature"), f"{prefix}.luaSignature")
        if not isinstance(raw_callback.get("allowNil"), bool):
            raise ValueError(f"{prefix}.allowNil must be boolean")
        thread_policy = _required_string(
            raw_callback.get("threadPolicy"), f"{prefix}.threadPolicy"
        )
        if thread_policy not in _THREAD_POLICIES:
            raise ValueError(f"{prefix}.threadPolicy is unsupported: {thread_policy}")
        directions = raw_callback.get("directions")
        if (
            not isinstance(directions, list)
            or not directions
            or any(
                not isinstance(direction, str) or direction not in _DIRECTIONS
                for direction in directions
            )
            or len(set(directions)) != len(directions)
        ):
            raise ValueError(f"{prefix}.directions is invalid")
        _validate_parameters(raw_callback.get("parameters"), f"{prefix}.parameters")
        _validate_returns(raw_callback.get("returns"), f"{prefix}.returns")
        _required_string(raw_callback.get("name"), f"{prefix}.name")
        if not isinstance(raw_callback.get("clearSetterOnQuiesce"), bool):
            raise ValueError(f"{prefix}.clearSetterOnQuiesce must be boolean")
        if "nativeType" in raw_callback:
            native_type = _required_string(
                raw_callback.get("nativeType"), f"{prefix}.nativeType"
            )
            if selector_kind == "alias" and native_type != cpp_name:
                raise ValueError(f"{prefix}.nativeType must match its alias selector")

        if selector_kind != "alias":
            continue
        previous = result.get(cpp_name)
        if previous is not None:
            raise ValueError(f"duplicate callback codec alias: {cpp_name}")
        result[cpp_name] = CallbackCodec(
            cpp_name=cpp_name,
            canonical_type=canonical_type,
            codec=codec,
            lua_type=lua_type,
            allow_nil=raw_callback["allowNil"],
            thread_policy=thread_policy,
            directions=frozenset(directions),
        )
    return result
