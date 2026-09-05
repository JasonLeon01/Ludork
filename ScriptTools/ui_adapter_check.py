from __future__ import annotations

import argparse
import ast
import hashlib
import json
import pathlib
import re

from ScriptTools import ui_control_registry
from ScriptTools.core_bindgen.annotations import (
    macro_invocations,
    split_macro_arguments,
)


class UiAdapterConsistencyError(RuntimeError):
    pass


def _clean_token(value: str) -> str:
    return re.sub(r"\\\s*\n", "", value).strip()


def _decode_string(value: str, source: str) -> str:
    token = _clean_token(value)
    try:
        result = ast.literal_eval(token)
    except (SyntaxError, ValueError) as exception:
        raise UiAdapterConsistencyError(
            f"{source} must be a string literal"
        ) from exception
    if not isinstance(result, str):
        raise UiAdapterConsistencyError(f"{source} must be a string literal")
    return result


def _parse_bool(value: str, source: str) -> bool:
    token = _clean_token(value)
    if token == "true":
        return True
    if token == "false":
        return False
    raise UiAdapterConsistencyError(f"{source} must be true or false")


def _function_invocations(
    text: str,
    name: str,
) -> list[tuple[str, int]]:
    pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
    result: list[tuple[str, int]] = []
    for match in pattern.finditer(text):
        opening = text.find("(", match.start())
        depth = 0
        quote = ""
        escaped = False
        for index in range(opening, len(text)):
            char = text[index]
            if quote:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == quote:
                    quote = ""
                continue
            if char in "\"'":
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    result.append((text[opening + 1 : index], match.start()))
                    break
        else:
            raise UiAdapterConsistencyError(f"unclosed {name} invocation")
    return result


def _is_macro_definition(text: str, position: int) -> bool:
    line_start = text.rfind("\n", 0, position) + 1
    return text[line_start:position].lstrip().startswith("#define")


def _property(
    property_id: str,
    value_type: str,
    required: bool,
    default: object,
    editor_only: bool,
) -> dict[str, object]:
    return {
        "id": property_id,
        "type": value_type,
        "required": required,
        "default": default,
        "editorOnly": editor_only,
    }


def _normalise_python_controls() -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for control in ui_control_registry.SYSTEM_CONTROLS:
        result.append(
            {
                "controlId": control["controlId"],
                "source": control["source"],
                "displayName": control["displayName"],
                "category": control["category"],
                "adapter": control["adapter"],
                "childPolicy": control["childPolicy"],
                "slotType": control["slotType"],
                "properties": [
                    _property(
                        str(property_data["id"]),
                        str(property_data["type"]),
                        bool(property_data["required"]),
                        property_data["default"],
                        property_data.get("editorOnly") is True,
                    )
                    for property_data in control["properties"]
                ],
            }
        )
    return result


def _cpp_property(value: str, source: str) -> dict[str, object]:
    match = re.fullmatch(
        r"(UI_CONTROL_PROPERTY|UI_CONTROL_EDITOR_PROPERTY|"
        r"UI_CONTROL_COMMON_PROPERTY)\s*\((.*)\)",
        _clean_token(value),
        re.DOTALL,
    )
    if match is None:
        raise UiAdapterConsistencyError(f"{source} has an invalid property declaration")
    arguments = split_macro_arguments(match.group(2))
    if len(arguments) != 4:
        raise UiAdapterConsistencyError(f"{source} property requires four arguments")
    default_json = _decode_string(
        arguments[3],
        f"{source} default",
    )
    try:
        default = json.loads(default_json)
    except json.JSONDecodeError as exception:
        raise UiAdapterConsistencyError(f"{source} default is not JSON") from exception
    return _property(
        _decode_string(arguments[0], f"{source} id"),
        _decode_string(arguments[1], f"{source} type"),
        _parse_bool(arguments[2], f"{source} required"),
        default,
        match.group(1) == "UI_CONTROL_EDITOR_PROPERTY",
    )


def _cpp_controls(project_root: pathlib.Path) -> list[dict[str, object]]:
    path = (
        project_root
        / "Engine"
        / "Source"
        / "Core"
        / "include"
        / "UI"
        / "UiControlAdapterDescriptors.hpp"
    )
    text = path.read_text(encoding="utf-8")
    common_start = text.index("#define UI_CONTROL_COMMON_PROPERTIES")
    common_end = text.index(
        "#define LUDORK_UI_CONTROL_DEFINITIONS",
        common_start,
    )
    common_text = text[common_start:common_end]
    common_properties = [
        _cpp_property(
            f"UI_CONTROL_COMMON_PROPERTY({arguments})",
            f"{path}: common property",
        )
        for arguments, position in _function_invocations(
            common_text,
            "UI_CONTROL_COMMON_PROPERTY",
        )
        if not _is_macro_definition(common_text, position)
    ]
    plain_text_start = text.index(
        "#define UI_CONTROL_PLAIN_TEXT_PROPERTIES",
        common_start,
    )
    plain_text_text = text[plain_text_start:common_end]

    def control_properties(
        values: list[str],
        source: str,
    ) -> list[dict[str, object]]:
        result: list[dict[str, object]] = []
        for value in values:
            group = re.fullmatch(
                r"UI_CONTROL_PLAIN_TEXT_PROPERTIES\s*\((.*)\)",
                value,
                re.DOTALL,
            )
            if group is None:
                result.append(_cpp_property(value, source))
                continue
            default_size = _clean_token(group.group(1))
            for arguments, _ in _function_invocations(
                plain_text_text,
                "UI_CONTROL_PROPERTY",
            ):
                property_value = arguments.replace(
                    "DEFAULT_SIZE",
                    default_size,
                )
                result.append(
                    _cpp_property(
                        f"UI_CONTROL_PROPERTY({property_value})",
                        source,
                    )
                )
        return result

    controls: list[dict[str, object]] = []
    for invocation in macro_invocations(text, ("UI_CONTROL",)):
        if _is_macro_definition(text, invocation.start):
            continue
        arguments = [
            _clean_token(argument)
            for argument in split_macro_arguments(invocation.arguments)
        ]
        if len(arguments) < 8:
            raise UiAdapterConsistencyError(
                f"{path} BIND_UI_CONTROL requires descriptor properties"
            )
        child_policy_token = arguments[5]
        slot_type_token = arguments[6]
        child_policy = {
            "UiChildPolicy::None": "none",
            "UiChildPolicy::Single": "single",
            "UiChildPolicy::Multiple": "multiple",
        }.get(child_policy_token)
        if child_policy is None:
            raise UiAdapterConsistencyError(
                f"{path} has unknown child policy {child_policy_token}"
            )
        slot_type = {
            "UiControlSlotType::None": None,
            "UiControlSlotType::Canvas": "canvas",
            "UiControlSlotType::List": "list",
        }.get(slot_type_token, object())
        if not (slot_type is None or isinstance(slot_type, str)):
            raise UiAdapterConsistencyError(
                f"{path} has unknown Slot type {slot_type_token}"
            )
        controls.append(
            {
                "controlId": _decode_string(
                    arguments[1],
                    f"{path} controlId",
                ),
                "source": "system",
                "displayName": _decode_string(
                    arguments[3],
                    f"{path} displayName",
                ),
                "category": _decode_string(
                    arguments[4],
                    f"{path} category",
                ),
                "adapter": _decode_string(
                    arguments[2],
                    f"{path} adapter",
                ),
                "childPolicy": child_policy,
                "slotType": slot_type,
                "properties": [
                    *common_properties,
                    *control_properties(
                        arguments[7:],
                        f"{path} {arguments[0]}",
                    ),
                ],
            }
        )
    return controls


def _csharp_scalar(value: str, source: str) -> object:
    token = _clean_token(value)
    if token == "string.Empty":
        return ""
    if token == "true":
        return True
    if token == "false":
        return False
    if re.fullmatch(r"-?\d+", token):
        return int(token)
    if re.fullmatch(r"-?(?:\d+\.\d*|\.\d+)", token):
        return float(token)
    if token.startswith('"'):
        return _decode_string(token, source)
    raise UiAdapterConsistencyError(f"{source} has unsupported scalar {token}")


def _csharp_default(value: str, source: str) -> object:
    token = _clean_token(value)
    if token == "null":
        return None
    wrapper = re.fullmatch(
        r"(JsonValue\.Create|jsonNumber|jsonArray)\s*\((.*)\)",
        token,
        re.DOTALL,
    )
    if wrapper is not None:
        inner = wrapper.group(2)
        if wrapper.group(1) == "JsonValue.Create":
            return _csharp_scalar(inner, source)
        encoded = _decode_string(inner, source)
        try:
            return json.loads(encoded)
        except json.JSONDecodeError as exception:
            raise UiAdapterConsistencyError(
                f"{source} contains invalid JSON"
            ) from exception
    array = re.fullmatch(
        r"new\s+JsonArray\s*\((.*)\)",
        token,
        re.DOTALL,
    )
    if array is not None:
        return [
            _csharp_scalar(item, source)
            for item in split_macro_arguments(array.group(1))
        ]
    raise UiAdapterConsistencyError(f"{source} has unsupported default {token}")


def _csharp_property(value: str, source: str) -> dict[str, object]:
    arguments = split_macro_arguments(value)
    if len(arguments) not in {5, 6}:
        raise UiAdapterConsistencyError(
            f"{source} property requires five or six arguments"
        )
    return _property(
        _decode_string(arguments[0], f"{source} id"),
        _decode_string(arguments[2], f"{source} type"),
        _parse_bool(arguments[3], f"{source} required"),
        _csharp_default(arguments[4], f"{source} default"),
        len(arguments) == 6 and _parse_bool(arguments[5], f"{source} editorOnly"),
    )


def _csharp_properties(
    text: str,
    source: str,
) -> list[dict[str, object]]:
    return [
        _csharp_property(arguments, source)
        for arguments, _ in _function_invocations(text, "property")
    ]


def _csharp_controls(
    repository_root: pathlib.Path,
) -> list[dict[str, object]]:
    path = repository_root / "Services" / "UiControlRegistryService.cs"
    text = path.read_text(encoding="utf-8")
    common_start = text.index("CommonProperties =")
    common_end = text.index(
        "private static readonly IReadOnlyList<UiControlDescriptor>",
        common_start,
    )
    common_properties = _csharp_properties(
        text[common_start:common_end],
        f"{path} CommonProperties",
    )
    controls_start = text.index(
        "private static IReadOnlyList<UiControlDescriptor> createSystemDescriptors()"
    )
    controls_end = text.index(
        "private static UiControlDescriptor descriptor(",
        controls_start,
    )
    plain_text_start = text.index(
        "private static IReadOnlyList<UiControlPropertyDescriptor> plainTextProperties(",
        controls_start,
    )
    plain_text_properties = [
        arguments
        for arguments, _ in _function_invocations(
            text[plain_text_start:controls_end],
            "property",
        )
    ]

    def descriptor_properties(value: str) -> list[dict[str, object]]:
        token = _clean_token(value)
        if not token.startswith("[") or not token.endswith("]"):
            raise UiAdapterConsistencyError(
                f"{path} descriptor properties must be a collection expression"
            )
        result: list[dict[str, object]] = []
        for item in split_macro_arguments(token[1:-1]):
            item = _clean_token(item)
            property_match = re.fullmatch(
                r"property\s*\((.*)\)",
                item,
                re.DOTALL,
            )
            if property_match is not None:
                result.append(
                    _csharp_property(
                        property_match.group(1),
                        f"{path} descriptor properties",
                    )
                )
                continue
            group = re.fullmatch(
                r"\.\.plainTextProperties\s*\((.*)\)",
                item,
                re.DOTALL,
            )
            if group is None:
                raise UiAdapterConsistencyError(
                    f"{path} has an unsupported descriptor property {item}"
                )
            character_size = _clean_token(group.group(1))
            for property_value in plain_text_properties:
                result.append(
                    _csharp_property(
                        property_value.replace(
                            "JsonValue.Create(characterSize)",
                            f"JsonValue.Create({character_size})",
                        ),
                        f"{path} plain text properties",
                    )
                )
        return result

    controls: list[dict[str, object]] = []
    for descriptor_value, _ in _function_invocations(
        text[controls_start:controls_end],
        "descriptor",
    ):
        arguments = split_macro_arguments(descriptor_value)
        if len(arguments) != 6:
            raise UiAdapterConsistencyError(f"{path} descriptor requires six arguments")
        properties = descriptor_properties(arguments[5])
        controls.append(
            {
                "controlId": _decode_string(
                    arguments[0],
                    f"{path} controlId",
                ),
                "source": "system",
                "displayName": _decode_string(
                    arguments[1],
                    f"{path} displayName",
                ),
                "category": _decode_string(
                    arguments[2],
                    f"{path} category",
                ),
                "adapter": _decode_string(
                    arguments[0],
                    f"{path} adapter",
                ),
                "childPolicy": _decode_string(
                    arguments[3],
                    f"{path} childPolicy",
                ),
                "slotType": (
                    None
                    if _clean_token(arguments[4]) == "null"
                    else _decode_string(
                        arguments[4],
                        f"{path} slotType",
                    )
                ),
                "properties": [
                    *common_properties,
                    *properties,
                ],
            }
        )
    return controls


def _resolve_roots(
    requested_root: pathlib.Path,
) -> tuple[pathlib.Path, pathlib.Path | None]:
    root = requested_root.expanduser().resolve()
    repository_project = root / "Sample"
    if (
        repository_project
        / "Engine"
        / "Source"
        / "Core"
        / "include"
        / "UI"
        / "UiControlAdapterDescriptors.hpp"
    ).is_file():
        return repository_project, root
    if (
        root / "Engine" / "Source" / "Core" / "include" / "UI" / "UiControlAdapterDescriptors.hpp"
    ).is_file():
        return root, None
    raise UiAdapterConsistencyError(
        f"UI adapter descriptors were not found under {root}"
    )


def _fingerprint(
    controls: list[dict[str, object]],
) -> str:
    lines: list[str] = []
    for control in sorted(
        controls,
        key=lambda item: str(item["controlId"]),
    ):
        line = "|".join(
            (
                str(control["controlId"]),
                str(control["adapter"]),
                str(control["childPolicy"]),
                ("" if control["slotType"] is None else str(control["slotType"])),
                "",
            )
        )
        properties = control["properties"]
        if not isinstance(properties, list):
            raise UiAdapterConsistencyError(
                f"{control['controlId']} properties must be an array"
            )
        for property_data in properties:
            if not isinstance(property_data, dict):
                raise UiAdapterConsistencyError(
                    f"{control['controlId']} property must be an object"
                )
            if property_data["editorOnly"] is True:
                continue
            line += (
                f"{property_data['id']}:{property_data['type']}:"
                f"{str(property_data['required']).lower()};"
            )
        lines.append(line + "\n")
    return hashlib.sha256("".join(lines).encode("utf-8")).hexdigest()


def _control_lookup(
    controls: list[dict[str, object]],
    source: str,
) -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    for control in controls:
        control_id = str(control["controlId"])
        if control_id in result:
            raise UiAdapterConsistencyError(
                f"{source} has duplicate controlId {control_id}"
            )
        result[control_id] = control
    return result


def _compare_controls(
    expected: list[dict[str, object]],
    actual: list[dict[str, object]],
    source: str,
) -> None:
    expected_lookup = _control_lookup(expected, "Python")
    actual_lookup = _control_lookup(actual, source)
    if expected_lookup.keys() != actual_lookup.keys():
        missing = sorted(expected_lookup.keys() - actual_lookup.keys())
        extra = sorted(actual_lookup.keys() - expected_lookup.keys())
        raise UiAdapterConsistencyError(
            f"{source} controlIds differ; missing={missing}, extra={extra}"
        )
    for control_id in sorted(expected_lookup):
        if expected_lookup[control_id] != actual_lookup[control_id]:
            raise UiAdapterConsistencyError(
                f"{source} descriptor differs for {control_id}"
            )


def verify_ui_adapters(repository_root: pathlib.Path) -> str:
    project_root, editor_root = _resolve_roots(repository_root)
    python_controls = _normalise_python_controls()
    cpp_controls = _cpp_controls(project_root)
    _compare_controls(python_controls, cpp_controls, "C++")
    fingerprints = {
        ui_control_registry.adapter_fingerprint(),
        _fingerprint(cpp_controls),
    }
    if editor_root is not None:
        csharp_controls = _csharp_controls(editor_root)
        _compare_controls(python_controls, csharp_controls, "C#")
        fingerprints.add(_fingerprint(csharp_controls))
        host_path = editor_root / "UiPreviewHost" / "src" / "UiPreviewHost.cpp"
        host = host_path.read_text(encoding="utf-8")
        if "uiControlAdapterFingerprint()" not in host:
            raise UiAdapterConsistencyError(
                f"{host_path} does not use the Engine adapter fingerprint"
            )
        if re.search(r'"[0-9a-f]{64}"', host) is not None:
            raise UiAdapterConsistencyError(
                f"{host_path} contains a hard-coded fingerprint"
            )
    if len(fingerprints) != 1:
        raise UiAdapterConsistencyError(
            f"adapter fingerprints differ: {sorted(fingerprints)}"
        )
    return fingerprints.pop()


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools ui-adapter-check")
    parser.add_argument(
        "repository_root",
        nargs="?",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parent.parent,
    )
    parsed = parser.parse_args(arguments)
    try:
        fingerprint = verify_ui_adapters(parsed.repository_root)
    except (OSError, ValueError, UiAdapterConsistencyError) as exception:
        parser.exit(1, f"{exception}\n")
    print(f"UI adapter descriptors are consistent: {fingerprint}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
