from __future__ import annotations

import argparse
import json
import pathlib
import re

from .resource_constants import ASSET_PATH_PREFIX
from .ui_control_registry import UiControlRegistry, load_registry
from .ui_property_values import (
    INT32_MAX,
    INT32_MIN,
    UiAssetError,
    _finite_number,
    _float_number,
    _integer,
    _pair,
    _validate_integer_array,
    _validate_property_type,
)


ASSETS_RELATIVE_PATH = pathlib.Path("Data") / "UI" / "Assets"


def _is_link(path: pathlib.Path) -> bool:
    if path.is_symlink():
        return True
    is_junction = getattr(path, "is_junction", None)
    return bool(is_junction is not None and is_junction())


ASSET_FIELDS = {
    "type",
    "designSize",
    "palette",
    "root",
    "animations",
}
DESIGN_SIZE_FIELDS = {"width", "height"}
PALETTE_FIELDS = {"exposed", "displayName", "category"}
NODE_FIELDS = {
    "name",
    "controlId",
    "properties",
    "slot",
    "editor",
    "children",
}
EDITOR_FIELDS = {"previewText"}
CANVAS_SLOT_FIELDS = {
    "anchors",
    "offsets",
    "alignment",
    "autoSize",
    "zOrder",
}
ANCHOR_FIELDS = {"min", "max"}
OFFSET_FIELDS = {"left", "top", "right", "bottom"}
ANIMATION_FIELDS = {"name", "target", "duration", "pivot", "tracks"}
ANIMATION_TRACK_FIELDS = {"translation", "rotation", "scale", "colour"}
ANIMATION_KEY_FIELDS = {"time", "value"}


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"Invalid JSON constant: {value}")


def _load_json(path: pathlib.Path) -> dict[str, object]:
    try:
        value = json.loads(
            path.read_text(encoding="utf-8-sig"),
            parse_constant=_reject_json_constant,
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError) as exception:
        raise UiAssetError(f"Invalid JSON file: {path}") from exception
    if not isinstance(value, dict):
        raise UiAssetError(f"JSON root must be an object: {path}")
    return value


def _validate_asset_key(value: object, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise UiAssetError(f"{label} must be a non-empty UI asset key")
    if value.strip() != value or "\\" in value:
        raise UiAssetError(
            f"{label} must use a canonical UI asset key with forward slashes"
        )
    key = pathlib.PurePosixPath(value)
    if (
        key.is_absolute()
        or pathlib.PureWindowsPath(value).is_absolute()
        or key.as_posix() != value
        or value.endswith("/")
        or any(part in {"", ".", ".."} for part in value.split("/"))
        or ":" in value
    ):
        raise UiAssetError(f"{label} must be a relative UI asset key")
    if "." in key.name:
        raise UiAssetError(f"{label} must not include an extension")
    prefixes = (
        "Assets",
        "UI/Assets",
        "Data/UI/Assets",
    )
    if any(value == prefix or value.startswith(prefix + "/") for prefix in prefixes):
        raise UiAssetError(
            f"{label} must be relative to Data/UI/Assets without a prefix"
        )
    return value


def _asset_key_from_path(
    path: pathlib.Path,
    assets_root: pathlib.Path,
) -> str:
    relative = path.relative_to(assets_root).with_suffix("")
    return _validate_asset_key(
        pathlib.PurePosixPath(*relative.parts).as_posix(),
        str(path),
    )


def _reject_unknown_fields(
    value: dict[str, object],
    allowed: set[str],
    label: str,
) -> None:
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise UiAssetError(f"{label} has unknown field {unknown[0]}")


def _validate_property_semantics(
    control_id: str,
    property_id: str,
    value: object,
    label: str,
) -> None:
    if (
        control_id == "Engine.ListView"
        and property_id == "columns"
        and _integer(value, label) <= 0
    ):
        raise UiAssetError(f"{label} must be positive")
    if (
        control_id == "Engine.TabView"
        and property_id == "items"
        and isinstance(value, list)
        and not value
    ):
        raise UiAssetError(f"{label} must not be empty")
    if (
        control_id == "Engine.WrapBox"
        and property_id == "count"
        and _integer(value, label) < 0
    ):
        raise UiAssetError(f"{label} cannot be negative")
    if (
        control_id in {"Engine.Canvas", "Engine.Window"}
        and property_id == "size"
        and isinstance(value, list)
        and any(
            _integer(component, f"{label}[{index}]") > INT32_MAX
            for index, component in enumerate(value)
        )
    ):
        raise UiAssetError(f"{label} components must not exceed {INT32_MAX}")
    if (
        (
            property_id == "scale"
            or control_id == "Engine.WrapBox" and property_id == "size"
        )
        and isinstance(value, list)
        and any(
            _float_number(component, f"{label}[{index}]") < 0.0
            for index, component in enumerate(value)
        )
    ):
        raise UiAssetError(f"{label} components cannot be negative")
    ranges = {
        "characterSize": (1.0, 512.0),
        "slantAngle": (-45.0, 45.0),
        "letterSpacing": (0.1, 10.0),
        "lineSpacing": (0.1, 10.0),
        "outlineThickness": (0.0, 32.0),
        "glowRadius": (0.0, 64.0),
        "glowIntensity": (0.0, 1.0),
    }
    if property_id in ranges:
        minimum, maximum = ranges[property_id]
        number = _finite_number(value, label)
        if number < minimum or number > maximum:
            raise UiAssetError(f"{label} must be between {minimum:g} and {maximum:g}")


def _safe_project_path(
    project_root: pathlib.Path,
    relative_value: str,
    label: str,
) -> pathlib.Path:
    relative = pathlib.PurePosixPath(relative_value.replace("\\", "/"))
    if relative.is_absolute() or ".." in relative.parts:
        raise UiAssetError(f"{label} must be a project-relative path")
    path = (project_root / pathlib.Path(*relative.parts)).resolve()
    try:
        path.relative_to(project_root)
    except ValueError as exception:
        raise UiAssetError(f"{label} escapes the project directory") from exception
    return path


def _game_asset_path(
    project_root: pathlib.Path,
    value: str,
    label: str,
) -> tuple[pathlib.Path, pathlib.PurePosixPath]:
    prefix = ASSET_PATH_PREFIX
    if value != value.strip() or "\\" in value or not value.startswith(prefix):
        raise UiAssetError(f"{label} must use a canonical /Game/Assets/ path")
    relative_text = value[len(prefix) :]
    relative = pathlib.PurePosixPath(relative_text)
    if (
        relative.is_absolute()
        or str(relative) != relative_text
        or not relative.suffix
        or not relative.parts
        or relative.parts[0].casefold().endswith(".ldpak")
        or any(part in {"", ".", ".."} or "\0" in part for part in relative.parts)
    ):
        raise UiAssetError(f"{label} must use a canonical /Game/Assets/ path")
    path = _safe_project_path(
        project_root,
        str(pathlib.PurePosixPath("Assets", relative)),
        label,
    )
    return path, relative


def _is_exact_case_asset_file(
    project_root: pathlib.Path,
    relative: pathlib.PurePosixPath,
) -> bool:
    current = project_root / "Assets"
    if not project_root.is_dir() or not any(
        child.name == "Assets" and not _is_link(child) and child.is_dir()
        for child in project_root.iterdir()
    ):
        return False
    for part in relative.parts:
        if not current.is_dir():
            return False
        match = next((child for child in current.iterdir() if child.name == part), None)
        if match is None or _is_link(match):
            return False
        current = match
    return current.is_file()


def _validate_property_reference(
    project_root: pathlib.Path,
    registry: UiControlRegistry,
    control_id: str,
    property_id: str,
    value: object,
    label: str,
) -> None:
    if not isinstance(value, str) or value == "":
        return
    text = value
    if property_id in {
        "texture",
        "backgroundTexture",
        "fillTexture",
        "windowSkin",
        "lineTexture",
        "handleTexture",
        "font",
    }:
        path, relative = _game_asset_path(project_root, text, label)
        if not path.is_file() or not _is_exact_case_asset_file(project_root, relative):
            raise UiAssetError(f"{label} resource was not found: {text}")
        return
    if property_id == "shader":
        path, key = _game_asset_path(project_root, text, label)
        if not key.parts or key.parts[0] != "Shaders":
            raise UiAssetError(
                f"{label} must use a canonical /Game/Assets/Shaders/ path"
            )
        if not path.is_file() or not _is_exact_case_asset_file(project_root, key):
            raise UiAssetError(f"{label} shader was not found: {text}")
        return
    if property_id in {"textConfig", "opacityCurve", "gradientCurve"}:
        section = "TextConfigs" if property_id == "textConfig" else "Curves"
        key = pathlib.PurePosixPath(text)
        if (
            key.is_absolute()
            or str(key) != text
            or "." in key.name
            or any(part in {".", ".."} for part in key.parts)
            or text.startswith(f"{section}/")
            or text.startswith(f"Data.{section}.")
        ):
            raise UiAssetError(
                f"{label} must use a canonical {section} key without an extension"
            )
        relative = pathlib.PurePosixPath("Data", section, key)
        path = _safe_project_path(project_root, str(relative), label)
        candidates = (
            path.with_suffix(".json"),
            path.with_suffix(".ldc"),
        )
        if not any(candidate.is_file() for candidate in candidates):
            raise UiAssetError(f"{label} {section} resource was not found: {text}")
        json_path = candidates[0]
        if not json_path.is_file():
            return
        data = _load_json(json_path)
        if property_id == "textConfig":
            text_kind = registry.text_kind(control_id)
            expected_type = None if text_kind is None else text_kind + "TextConfig"
            if expected_type is not None and data.get("type") != expected_type:
                raise UiAssetError(f"{label} must reference a {expected_type}")
        elif data.get("type") != "curve":
            raise UiAssetError(f"{label} must reference a scalar curve")


def _validate_canvas_slot(slot: dict[str, object], label: str) -> None:
    _reject_unknown_fields(slot, CANVAS_SLOT_FIELDS, label)
    anchors = slot.get("anchors")
    offsets = slot.get("offsets")
    if not isinstance(anchors, dict):
        raise UiAssetError(f"{label}.anchors must be an object")
    _reject_unknown_fields(anchors, ANCHOR_FIELDS, f"{label}.anchors")
    minimum = _pair(anchors.get("min"), f"{label}.anchors.min")
    maximum = _pair(anchors.get("max"), f"{label}.anchors.max")
    for index in range(2):
        if minimum[index] < 0.0 or maximum[index] > 1.0:
            raise UiAssetError(f"{label}.anchors must stay within 0..1")
        if minimum[index] > maximum[index]:
            raise UiAssetError(f"{label}.anchors.min must not exceed max")
    if not isinstance(offsets, dict):
        raise UiAssetError(f"{label}.offsets must be an object")
    _reject_unknown_fields(offsets, OFFSET_FIELDS, f"{label}.offsets")
    for key in ("left", "top", "right", "bottom"):
        _float_number(offsets.get(key), f"{label}.offsets.{key}")
    alignment = _pair(slot.get("alignment"), f"{label}.alignment")
    if any(component < 0.0 or component > 1.0 for component in alignment):
        raise UiAssetError(f"{label}.alignment must stay within 0..1")
    if not isinstance(slot.get("autoSize"), bool):
        raise UiAssetError(f"{label}.autoSize must be a boolean")
    z_order = _integer(slot.get("zOrder"), f"{label}.zOrder")
    if z_order < INT32_MIN or z_order > INT32_MAX:
        raise UiAssetError(f"{label}.zOrder must be a signed 32-bit integer")


def _validate_node(
    node: object,
    path: str,
    root: bool,
    parent: dict[str, object] | None,
    project_root: pathlib.Path,
    names: set[str],
    asset_references: list[str],
    registry: UiControlRegistry | None,
) -> None:
    if not isinstance(node, dict):
        raise UiAssetError(f"{path} must be an object")
    _reject_unknown_fields(node, NODE_FIELDS, path)
    name = node.get("name")
    if not isinstance(name, str) or not name.strip():
        raise UiAssetError(f"{path}.name must be a non-empty string")
    if name in names:
        raise UiAssetError(f"Duplicate node name: {name}")
    names.add(name)
    control_id = node.get("controlId")
    if not isinstance(control_id, str) or not control_id:
        raise UiAssetError(f"{path}.controlId must be a non-empty string")
    if (
        not control_id.startswith("Project:")
        and re.fullmatch(r"[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)+", control_id) is None
    ):
        raise UiAssetError(f"{path}.controlId must be a canonical native class path")
    properties = node.get("properties")
    if not isinstance(properties, dict):
        raise UiAssetError(f"{path}.properties must be an object")
    children = node.get("children")
    if not isinstance(children, list):
        raise UiAssetError(f"{path}.children must be an array")
    if root:
        if "slot" in node:
            raise UiAssetError(f"{path} root must not declare slot")
    else:
        slot = node.get("slot")
        if not isinstance(slot, dict):
            raise UiAssetError(f"{path}.slot must be an object")
        slot_type = None if parent is None else parent.get("slotType")
        if registry is None:
            if slot:
                _validate_canvas_slot(slot, f"{path}.slot")
        elif slot_type == "canvas":
            _validate_canvas_slot(slot, f"{path}.slot")
        elif slot_type == "list":
            if slot:
                raise UiAssetError(f"{path}.slot must be empty under ListView")
        else:
            raise UiAssetError(f"{path} parent does not provide a valid Slot type")

    descriptor: dict[str, object]
    if control_id.startswith("Project:"):
        reference = _validate_asset_key(
            control_id.removeprefix("Project:"),
            f"{path}.controlId",
        )
        asset_references.append(reference)
        if properties or children:
            raise UiAssetError(
                f"{path} nested asset cannot override properties or children"
            )
        descriptor = {
            "controlId": control_id,
            "source": "project",
            "childPolicy": "none",
            "slotType": None,
            "properties": [],
        }
    elif registry is None:
        descriptor = {"source": "system", "childPolicy": "multiple", "properties": []}
    else:
        descriptor = registry.control_lookup.get(control_id, {})
        if not descriptor:
            raise UiAssetError(f"{path} has unknown controlId {control_id}")
        property_descriptors = descriptor.get("properties")
        if not isinstance(property_descriptors, list):
            raise UiAssetError(f"{control_id} has invalid property descriptors")
        property_lookup = {
            str(property_data["id"]): property_data
            for property_data in property_descriptors
            if isinstance(property_data, dict)
        }
        for property_id, property_value in properties.items():
            property_data = property_lookup.get(property_id)
            if property_data is None:
                raise UiAssetError(
                    f"{path}.properties has unknown property {property_id}"
                )
            if property_data.get("editorOnly") is True:
                raise UiAssetError(f"{path}.properties.{property_id} is editor-only")
            _validate_property_type(
                property_value,
                str(property_data["type"]),
                f"{path}.properties.{property_id}",
                property_data["type"] == "sf.IntRect",
            )
            _validate_property_semantics(
                control_id,
                property_id,
                property_value,
                f"{path}.properties.{property_id}",
            )
            _validate_property_reference(
                project_root,
                registry,
                control_id,
                property_id,
                property_value,
                f"{path}.properties.{property_id}",
            )
        for property_id, property_data in property_lookup.items():
            if (
                property_data.get("editorOnly") is not True
                and property_data.get("required") is True
                and property_id not in properties
            ):
                raise UiAssetError(
                    f"{path}.properties is missing required property {property_id}"
                )
        if properties.get("gradientEnabled") is True and not properties.get(
            "gradientCurve"
        ):
            raise UiAssetError(
                f"{path}.properties.gradientCurve is required when gradientEnabled is true"
            )
    editor = node.get("editor")
    if "editor" in node:
        if not isinstance(editor, dict):
            raise UiAssetError(f"{path}.editor must be an object")
        _reject_unknown_fields(editor, EDITOR_FIELDS, f"{path}.editor")
        if descriptor.get("source") == "project" and editor:
            raise UiAssetError(f"{path} nested asset cannot override editor data")
        has_preview_text = "previewText" in editor
        preview_text = editor.get("previewText")
        if has_preview_text and not isinstance(preview_text, str):
            raise UiAssetError(f"{path}.editor.previewText must be a string")
        preview_property = next(
            (
                property_data
                for property_data in descriptor.get("properties", [])
                if isinstance(property_data, dict)
                and property_data.get("id") == "previewText"
                and property_data.get("editorOnly") is True
            ),
            None,
        )
        if has_preview_text and preview_property is None and registry is not None:
            raise UiAssetError(
                f"{path}.editor.previewText is only valid on text controls"
            )
        if has_preview_text and preview_property is not None:
            _validate_property_type(
                preview_text,
                str(preview_property["type"]),
                f"{path}.editor.previewText",
                False,
            )
    child_policy = descriptor.get("childPolicy")
    if child_policy == "none" and children:
        raise UiAssetError(f"{path} control cannot contain children")
    if (child_policy == "single" or control_id == "Engine.WrapBox") and len(children) > 1:
        raise UiAssetError(f"{path} control accepts only one child")
    if child_policy not in {"none", "single", "multiple"}:
        raise UiAssetError(f"{path} has an invalid child policy")
    for index, child in enumerate(children):
        _validate_node(
            child,
            f"{path}.children[{index}]",
            False,
            descriptor,
            project_root,
            names,
            asset_references,
            registry,
        )


def _reference_nodes(node: dict[str, object]) -> list[dict[str, object]]:
    result = [node]
    control_id = str(node["controlId"])
    if control_id != "Engine.WrapBox" and not control_id.startswith("Project:"):
        for child in node["children"]:
            result.extend(_reference_nodes(child))
    return result


def _validate_animation_track(
    value: object,
    label: str,
    duration: float,
    vector: bool,
    track: str,
) -> None:
    if not isinstance(value, list) or not value:
        raise UiAssetError(f"{label} must contain at least one key")
    previous = -1.0
    for index, key_value in enumerate(value):
        key_label = f"{label}[{index}]"
        if not isinstance(key_value, dict):
            raise UiAssetError(f"{key_label} must be an object")
        _reject_unknown_fields(key_value, ANIMATION_KEY_FIELDS, key_label)
        time = _float_number(key_value.get("time"), f"{key_label}.time")
        if time < 0.0 or time > duration or time <= previous:
            raise UiAssetError(
                f"{key_label}.time must be strictly ordered within duration"
            )
        previous = time
        if track == "colour":
            _validate_integer_array(
                key_value.get("value"), 4, f"{key_label}.value", colour=True
            )
        elif vector:
            components = _pair(key_value.get("value"), f"{key_label}.value")
            if track == "scale" and any(component < 0.0 for component in components):
                raise UiAssetError(f"{key_label}.value cannot be negative")
        else:
            _float_number(key_value.get("value"), f"{key_label}.value")


def _validate_animations(
    value: object,
    label: str,
    node_names: set[str],
) -> None:
    if value is None:
        return
    if not isinstance(value, list):
        raise UiAssetError(f"{label} must be an array")
    bindings: set[tuple[str | None, str]] = set()
    for index, animation_value in enumerate(value):
        animation_label = f"{label}[{index}]"
        if not isinstance(animation_value, dict):
            raise UiAssetError(f"{animation_label} must be an object")
        _reject_unknown_fields(animation_value, ANIMATION_FIELDS, animation_label)
        name = animation_value.get("name")
        if not isinstance(name, str) or not name.strip():
            raise UiAssetError(f"{animation_label}.name must be non-empty")
        if "target" not in animation_value:
            raise UiAssetError(f"{animation_label}.target is required")
        target = animation_value.get("target")
        if target is not None and (
            not isinstance(target, str) or not target or target not in node_names
        ):
            raise UiAssetError(
                f"{animation_label}.target must name a local UI node or be null"
            )
        binding = (target, name)
        if binding in bindings:
            raise UiAssetError(
                f"{animation_label} duplicates an animation name and target"
            )
        bindings.add(binding)
        duration = _float_number(
            animation_value.get("duration"), f"{animation_label}.duration"
        )
        if duration <= 0.0:
            raise UiAssetError(f"{animation_label}.duration must be positive")
        if "pivot" in animation_value:
            pivot = _pair(animation_value["pivot"], f"{animation_label}.pivot")
            if any(component < 0.0 or component > 1.0 for component in pivot):
                raise UiAssetError(f"{animation_label}.pivot must stay within 0..1")
        tracks = animation_value.get("tracks")
        if not isinstance(tracks, dict):
            raise UiAssetError(f"{animation_label}.tracks must be an object")
        _reject_unknown_fields(
            tracks, ANIMATION_TRACK_FIELDS, f"{animation_label}.tracks"
        )
        for track, keys in tracks.items():
            _validate_animation_track(
                keys,
                f"{animation_label}.tracks.{track}",
                duration,
                track in {"translation", "scale"},
                track,
            )


def _validate_asset(
    path: pathlib.Path,
    value: dict[str, object],
    project_root: pathlib.Path,
    registry: UiControlRegistry | None,
) -> list[str]:
    _reject_unknown_fields(value, ASSET_FIELDS, str(path))
    if value.get("type") != "uiAsset":
        raise UiAssetError(f"UI asset type must be uiAsset: {path}")
    design_size = value.get("designSize")
    if not isinstance(design_size, dict):
        raise UiAssetError(f"{path}.designSize must be an object")
    _reject_unknown_fields(
        design_size,
        DESIGN_SIZE_FIELDS,
        f"{path}.designSize",
    )
    width = _float_number(design_size.get("width"), f"{path}.designSize.width")
    height = _float_number(
        design_size.get("height"),
        f"{path}.designSize.height",
    )
    if width <= 0.0 or height <= 0.0 or width > INT32_MAX or height > INT32_MAX:
        raise UiAssetError(
            f"{path}.designSize must be positive and not exceed {INT32_MAX}"
        )
    palette = value.get("palette")
    if not isinstance(palette, dict):
        raise UiAssetError(f"{path}.palette must be an object")
    _reject_unknown_fields(palette, PALETTE_FIELDS, f"{path}.palette")
    if not isinstance(palette.get("exposed"), bool):
        raise UiAssetError(f"{path}.palette.exposed must be a boolean")
    for key in ("displayName", "category"):
        if not isinstance(palette.get(key), str) or not palette[key].strip():
            raise UiAssetError(f"{path}.palette.{key} must be non-empty")
    references: list[str] = []
    node_names: set[str] = set()
    _validate_node(
        value.get("root"),
        f"{path}.root",
        True,
        None,
        project_root,
        node_names,
        references,
        registry,
    )
    animation_targets = {
        str(node["name"]) for node in _reference_nodes(value["root"])
    }
    _validate_animations(value.get("animations"), f"{path}.animations", animation_targets)
    return references


def _validate_reference_graph(
    references: dict[str, list[str]],
    exposed_assets: set[str],
) -> None:
    states: dict[str, int] = {}
    stack: list[str] = []

    def visit(asset_key: str) -> None:
        state = states.get(asset_key, 0)
        if state == 2:
            return
        if state == 1:
            start = stack.index(asset_key)
            cycle = " -> ".join([*stack[start:], asset_key])
            raise UiAssetError(f"UI asset cycle: {cycle}")
        states[asset_key] = 1
        stack.append(asset_key)
        for target in references[asset_key]:
            chain = " -> ".join([*stack, target])
            if target not in references:
                raise UiAssetError(f"Missing nested UI asset reference: {chain}")
            if target not in exposed_assets:
                raise UiAssetError(f"Nested UI asset is not exposed: {chain}")
            visit(target)
        stack.pop()
        states[asset_key] = 2

    for asset_key in references:
        visit(asset_key)


def validate_assets(
    project_root: pathlib.Path,
    registry: UiControlRegistry | pathlib.Path | None = None,
    *,
    structure_only: bool = False,
) -> None:
    root = project_root.expanduser().resolve()
    if not structure_only:
        if registry is None:
            from .ui_preview import ensure_preview

            registry = ensure_preview(root).registry
        elif isinstance(registry, pathlib.Path):
            registry = load_registry(registry)
    else:
        registry = None
    assets_root = root / ASSETS_RELATIVE_PATH
    references: dict[str, list[str]] = {}
    exposed_assets: set[str] = set()
    if assets_root.is_dir():
        candidates = sorted(
            path
            for path in assets_root.rglob("*")
            if path.is_file()
            and (path.suffix.lower() == ".json" or path.name.lower() == ".json")
        )
        for path in candidates:
            if (path.suffix.lower() == ".json" and path.suffix != ".json") or (
                path.name.lower() == ".json" and path.name != ".json"
            ):
                raise UiAssetError(
                    f"UI asset file extension must be lowercase .json: {path}"
                )
            asset_key = _asset_key_from_path(path, assets_root)
            if asset_key in references:
                raise UiAssetError(f"Duplicate UI asset key: {asset_key}")
            value = _load_json(path)
            references[asset_key] = _validate_asset(
                path,
                value,
                root,
                registry,
            )
            palette = value["palette"]
            if isinstance(palette, dict) and palette["exposed"] is True:
                exposed_assets.add(asset_key)
    _validate_reference_graph(references, exposed_assets)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools ui-assets")
    operations = parser.add_subparsers(dest="operation", required=True)
    validate_parser = operations.add_parser("validate")
    validate_parser.add_argument("project_root", type=pathlib.Path)
    validate_parser.add_argument("--registry", type=pathlib.Path)
    validate_parser.add_argument("--structure-only", action="store_true")
    generate_parser = operations.add_parser("generate")
    generate_parser.add_argument("project_root", type=pathlib.Path)
    generate_parser.add_argument("--check", action="store_true")
    manifest_parser = operations.add_parser("manifest")
    manifest_parser.add_argument("project_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "manifest":
            from .ui_asset_generation import generation_manifest

            print(json.dumps(generation_manifest(parsed.project_root), ensure_ascii=False))
            return 0
        if parsed.operation == "generate":
            from .ui_asset_generation import generate_assets

            changes = generate_assets(parsed.project_root, check=parsed.check)
            if parsed.check and changes:
                print("Generated UI code is out of date:")
                for path in changes:
                    print(path)
                return 1
            print(
                f"Generated UI code updated ({len(changes)} files)"
                if changes
                else "Generated UI code is up to date"
            )
            return 0
        validate_assets(
            parsed.project_root, parsed.registry, structure_only=parsed.structure_only
        )
        print(
            "UI asset structure is valid; native validation is pending"
            if parsed.structure_only
            else "UI assets are valid"
        )
    except UiAssetError as exception:
        parser.exit(1, f"{exception}\n")
    return 0
