from __future__ import annotations

import hashlib
import json
import pathlib
import re
from dataclasses import dataclass, field

from .ui_property_values import UiAssetError, _validate_property_type


class UiRegistryError(UiAssetError):
    pass


def strict_json(data: bytes) -> object:
    def constant(value: str) -> None:
        raise UiRegistryError(f"Invalid JSON constant: {value}")

    def object_pairs(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise UiRegistryError(f"Duplicate JSON field: {key}")
            result[key] = value
        return result

    try:
        return json.loads(
            data.decode("utf-8"),
            parse_constant=constant,
            object_pairs_hook=object_pairs,
        )
    except (UnicodeError, ValueError) as error:
        raise UiRegistryError(f"Invalid UI registry JSON: {error}") from error


def require_fields(value: object, fields: set[str], label: str) -> dict[str, object]:
    if not isinstance(value, dict) or value.keys() != fields:
        raise UiRegistryError(
            f"{label} must contain exactly: {', '.join(sorted(fields))}"
        )
    return value


def require_string(value: object, label: str) -> str:
    if (
        not isinstance(value, str)
        or not value
        or value != value.strip()
        or "\0" in value
    ):
        raise UiRegistryError(
            f"{label} must be a non-empty string without surrounding whitespace or NUL"
        )
    return value


def adapter_fingerprint(
    controls: tuple[dict[str, object], ...] | list[dict[str, object]],
) -> str:
    lines: list[str] = []
    for control in sorted(controls, key=lambda item: item["controlId"]):
        line = "|".join(
            (
                control["controlId"],
                control["adapter"],
                control["childPolicy"],
                control["slotType"] or "",
                "",
            )
        )
        for prop in control["properties"]:
            if not prop["editorOnly"]:
                line += f"{prop['id']}:{prop['type']}:{str(prop['required']).lower()};"
        lines.append(line + "\n")
    return hashlib.sha256("".join(lines).encode("utf-8")).hexdigest()


@dataclass(frozen=True)
class UiControlRegistry:
    controls: tuple[dict[str, object], ...]
    fingerprint: str
    digest: str
    raw: bytes = field(repr=False)
    control_lookup: dict[str, dict[str, object]] = field(init=False, repr=False)

    def __post_init__(self) -> None:
        object.__setattr__(
            self,
            "control_lookup",
            {control["controlId"]: control for control in self.controls},
        )

    def text_kind(self, control_id: str) -> str | None:
        return self.control_lookup.get(control_id, {}).get("textKind")


def read_registry(data: bytes) -> UiControlRegistry:
    document = require_fields(
        strict_json(data),
        {"formatVersion", "adapterFingerprint", "controls"},
        "UI registry",
    )
    if type(document["formatVersion"]) is not int or document["formatVersion"] != 1:
        raise UiRegistryError(
            "Unsupported UI registry format; rebuild the project preview"
        )
    controls = document["controls"]
    if not isinstance(controls, list) or not controls:
        raise UiRegistryError("UI registry must contain controls")
    seen: set[str] = set()
    for item in controls:
        control = require_fields(
            item,
            {
                "controlId",
                "source",
                "adapter",
                "displayName",
                "category",
                "childPolicy",
                "slotType",
                "textKind",
                "properties",
            },
            "Control descriptor",
        )
        control_id = require_string(control["controlId"], "controlId")
        if (
            not re.fullmatch(r"[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)+", control_id)
            or control_id in seen
        ):
            raise UiRegistryError(f"Invalid or duplicate controlId: {control_id}")
        seen.add(control_id)
        if control["source"] != "system" or control["adapter"] != control_id:
            raise UiRegistryError(
                f"{control_id} must describe its canonical native adapter"
            )
        for name in ("displayName", "category"):
            require_string(control[name], f"{control_id}.{name}")
        if control["childPolicy"] not in ("none", "single", "multiple") or control[
            "slotType"
        ] not in (None, "canvas", "list"):
            raise UiRegistryError(f"Invalid child policy or slot type: {control_id}")
        if (control["childPolicy"] == "none") != (control["slotType"] is None):
            raise UiRegistryError(
                f"Inconsistent child policy and slot type: {control_id}"
            )
        if control["textKind"] not in (None, "plain", "rich"):
            raise UiRegistryError(f"Invalid textKind: {control_id}")
        properties = control["properties"]
        if not isinstance(properties, list):
            raise UiRegistryError(f"Invalid properties: {control_id}")
        property_ids: set[str] = set()
        for item in properties:
            prop = require_fields(
                item,
                {
                    "id",
                    "displayName",
                    "type",
                    "required",
                    "default",
                    "editorOnly",
                    "adapterProperty",
                },
                f"{control_id} property",
            )
            property_id = require_string(prop["id"], "property id")
            if property_id in property_ids:
                raise UiRegistryError(
                    f"Duplicate property ID: {control_id}.{property_id}"
                )
            property_ids.add(property_id)
            require_string(
                prop["displayName"], f"{control_id}.{property_id}.displayName"
            )
            require_string(prop["type"], f"{control_id}.{property_id}.type")
            if any(
                type(prop[name]) is not bool
                for name in ("required", "editorOnly", "adapterProperty")
            ):
                raise UiRegistryError(
                    f"Invalid property flags: {control_id}.{property_id}"
                )
            if prop["editorOnly"] and prop["adapterProperty"]:
                raise UiRegistryError(
                    f"Editor-only property cannot belong to the adapter: {control_id}.{property_id}"
                )
            _validate_property_type(
                prop["default"],
                prop["type"],
                f"{control_id}.{property_id} default",
                prop["type"] == "sf.IntRect",
            )
        if control["textKind"] is not None and "textConfig" not in property_ids:
            raise UiRegistryError(
                f"Text control has no textConfig property: {control_id}"
            )
    fingerprint = adapter_fingerprint(controls)
    if document["adapterFingerprint"] != fingerprint:
        raise UiRegistryError(
            "UI registry adapter fingerprint does not match its descriptors"
        )
    return UiControlRegistry(
        tuple(controls), fingerprint, hashlib.sha256(data).hexdigest(), data
    )


def load_registry(path: pathlib.Path) -> UiControlRegistry:
    try:
        return read_registry(pathlib.Path(path).read_bytes())
    except OSError as error:
        raise UiRegistryError(
            f"UI registry is unavailable: {path}. Build the C++ project or update the Standalone preview."
        ) from error
