from __future__ import annotations

import hashlib
from collections.abc import Iterable


class UiControlRegistryError(RuntimeError):
    pass


def _property(
    property_id: str,
    value_type: str,
    default: object,
    editor_only: bool = False,
) -> dict[str, object]:
    result: dict[str, object] = {
        "id": property_id,
        "type": value_type,
        "required": False,
        "default": default,
    }
    if editor_only:
        result["editorOnly"] = True
    return result


COMMON_PROPERTIES = (
    _property("visible", "bool", True),
    _property("rotation", "float", 0.0),
    _property("scale", "sf.Vector2f", [1.0, 1.0]),
    _property("origin", "sf.Vector2f", [0.0, 0.0]),
)


def _system_control(
    control_id: str,
    display_name: str,
    category: str,
    child_policy: str,
    slot_type: str | None,
    properties: Iterable[dict[str, object]],
) -> dict[str, object]:
    return {
        "controlId": control_id,
        "source": "system",
        "displayName": display_name,
        "category": category,
        "adapter": control_id,
        "childPolicy": child_policy,
        "slotType": slot_type,
        "properties": [
            *COMMON_PROPERTIES,
            *properties,
        ],
    }


SYSTEM_CONTROLS = (
    _system_control(
        "Engine.Canvas",
        "Canvas",
        "Layout",
        "multiple",
        "canvas",
        (_property("size", "sf.Vector2u", [100, 100]),),
    ),
    _system_control(
        "Engine.ListView",
        "List View",
        "Layout",
        "multiple",
        "list",
        (
            _property("size", "sf.Vector2f", [100.0, 100.0]),
            _property("defaultItemHeight", "int", 32),
            _property("fixItemHeight", "bool", False),
            _property("columns", "int", 1),
        ),
    ),
    _system_control(
        "Engine.Window",
        "Window",
        "Visual",
        "none",
        None,
        (
            _property("size", "sf.Vector2u", [160, 96]),
            _property("windowSkin", "string", ""),
            _property("repeated", "bool", False),
        ),
    ),
    _system_control(
        "Engine.Rect",
        "Rect",
        "Visual",
        "none",
        None,
        (
            _property("size", "sf.Vector2f", [160.0, 96.0]),
            _property("windowSkin", "string", ""),
            _property("opacityCurve", "string", ""),
        ),
    ),
    _system_control(
        "Engine.SolidRect",
        "Solid Rect",
        "Visual",
        "none",
        None,
        (
            _property("size", "sf.Vector2f", [100.0, 32.0]),
            _property("fillColor", "sf.Color", [255, 255, 255, 255]),
            _property("outlineColor", "sf.Color", [0, 0, 0, 0]),
            _property("outlineThickness", "float", 0.0),
        ),
    ),
    _system_control(
        "Engine.ProgressBar",
        "Progress Bar",
        "Visual",
        "none",
        None,
        (
            _property("size", "sf.Vector2f", [100.0, 12.0]),
            _property("progress", "float", 0.0),
            _property("backgroundColor", "sf.Color", [255, 255, 255, 64]),
            _property("fillColor", "sf.Color", [255, 255, 255, 255]),
        ),
    ),
    _system_control(
        "Engine.Image",
        "Image",
        "Visual",
        "none",
        None,
        (
            _property("texture", "string", ""),
            _property("textureRect", "sf.IntRect", None),
            _property("colour", "sf.Color", [255, 255, 255, 255]),
        ),
    ),
    _system_control(
        "Engine.Button",
        "Button",
        "Input",
        "none",
        None,
        (
            _property("texture", "string", ""),
            _property("textureRect", "sf.IntRect", None),
            _property("colour", "sf.Color", [255, 255, 255, 255]),
            _property("hoverColour", "sf.Color", [255, 255, 255, 255]),
            _property("pressedColour", "sf.Color", [255, 255, 255, 255]),
        ),
    ),
    _system_control(
        "Engine.CheckBox",
        "Check Box",
        "Input",
        "none",
        None,
        (
            _property("size", "sf.Vector2f", [32.0, 32.0]),
            _property("checked", "bool", False),
            _property("windowSkin", "string", ""),
            _property("textConfig", "string", "UI/Text20"),
        ),
    ),
    _system_control(
        "Engine.TabView",
        "Tab View",
        "Input",
        "none",
        None,
        (
            _property("size", "sf.Vector2f", [100.0, 32.0]),
            _property("windowSkin", "string", ""),
            _property("textConfig", "string", "UI/Default"),
            _property("tabCount", "int", 1),
        ),
    ),
    _system_control(
        "Engine.Slider",
        "Slider",
        "Input",
        "none",
        None,
        (
            _property("size", "sf.Vector2f", [64.0, 8.0]),
            _property("minValue", "int", 0),
            _property("maxValue", "int", 100),
            _property("value", "int", 0),
            _property(
                "lineTexture",
                "string",
                "Assets/System/SliderLine.png",
            ),
            _property(
                "handleTexture",
                "string",
                "Assets/System/SliderHandle.png",
            ),
        ),
    ),
    _system_control(
        "Engine.DropBox",
        "Drop Box",
        "Input",
        "none",
        None,
        (
            _property("size", "sf.Vector2f", [200.0, 32.0]),
            _property("windowSkin", "string", ""),
            _property("textConfig", "string", "UI/Text20"),
            _property("previewText", "string", "Option", editor_only=True),
        ),
    ),
    _system_control(
        "Engine.FunctionalImage",
        "Functional Image",
        "Input",
        "none",
        None,
        (
            _property("texture", "string", ""),
            _property("textureRect", "sf.IntRect", None),
            _property("colour", "sf.Color", [255, 255, 255, 255]),
        ),
    ),
    _system_control(
        "Engine.FunctionalPlainText",
        "Functional Plain Text",
        "Input",
        "none",
        None,
        (
            _property("textConfig", "string", ""),
            _property("text", "string", ""),
            _property("previewText", "string", "", editor_only=True),
            _property("colour", "sf.Color", [255, 255, 255, 255]),
            _property("outlineColor", "sf.Color", None),
            _property("outlineThickness", "float", None),
        ),
    ),
    _system_control(
        "Engine.FunctionalRichText",
        "Functional Rich Text",
        "Input",
        "none",
        None,
        (
            _property("textConfig", "string", ""),
            _property("text", "string", ""),
            _property("previewText", "string", "", editor_only=True),
            _property("colour", "sf.Color", [255, 255, 255, 255]),
        ),
    ),
    _system_control(
        "Engine.PlainText",
        "Plain Text",
        "Text",
        "none",
        None,
        (
            _property("textConfig", "string", ""),
            _property("text", "string", ""),
            _property("previewText", "string", "", editor_only=True),
            _property("colour", "sf.Color", [255, 255, 255, 255]),
            _property("outlineColor", "sf.Color", None),
            _property("outlineThickness", "float", None),
        ),
    ),
    _system_control(
        "Engine.RichText",
        "Rich Text",
        "Text",
        "none",
        None,
        (
            _property("textConfig", "string", ""),
            _property("text", "string", ""),
            _property("previewText", "string", "", editor_only=True),
            _property("colour", "sf.Color", [255, 255, 255, 255]),
        ),
    ),
)

SYSTEM_CONTROL_LOOKUP = {
    str(control["controlId"]): control
    for control in SYSTEM_CONTROLS
}
PLAIN_TEXT_CONTROL_IDS = {
    "Engine.CheckBox",
    "Engine.DropBox",
    "Engine.TabView",
    "Engine.PlainText",
    "Engine.FunctionalPlainText",
}
RICH_TEXT_CONTROL_IDS = {
    "Engine.RichText",
    "Engine.FunctionalRichText",
}


def adapter_fingerprint() -> str:
    lines: list[str] = []
    for control in sorted(
        SYSTEM_CONTROLS,
        key=lambda item: str(item["controlId"]),
    ):
        lines.append(
            "|".join(
                (
                    str(control["controlId"]),
                    str(control["adapter"]),
                    str(control["childPolicy"]),
                    "" if control["slotType"] is None else str(control["slotType"]),
                    "",
                )
            )
        )
        properties = control["properties"]
        if not isinstance(properties, list):
            raise UiControlRegistryError(
                f"{control['controlId']} properties must be an array"
            )
        for property_data in properties:
            if not isinstance(property_data, dict):
                raise UiControlRegistryError(
                    f"{control['controlId']} property must be an object"
                )
            if property_data.get("editorOnly") is True:
                continue
            lines[-1] += (
                f"{property_data['id']}:{property_data['type']}:"
                f"{str(property_data['required']).lower()};"
            )
        lines[-1] += "\n"
    payload = "".join(lines).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()
