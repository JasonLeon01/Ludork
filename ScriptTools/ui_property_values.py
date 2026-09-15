from __future__ import annotations

import math

INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1
UINT32_MAX = 2**32 - 1
FLOAT32_MAX = 3.4028234663852886e38


class UiAssetError(RuntimeError):
    pass


def _finite_number(value: object, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise UiAssetError(f"{label} must be a finite number")
    try:
        number = float(value)
    except OverflowError as exception:
        raise UiAssetError(f"{label} must be a finite number") from exception
    if not math.isfinite(number):
        raise UiAssetError(f"{label} must be a finite number")
    return number


def _float_number(value: object, label: str) -> float:
    number = _finite_number(value, label)
    if number < -FLOAT32_MAX or number > FLOAT32_MAX:
        raise UiAssetError(f"{label} is outside the float range")
    return number


def _pair(value: object, label: str) -> tuple[float, float]:
    if not isinstance(value, list) or len(value) != 2:
        raise UiAssetError(f"{label} must be a two-item array")
    return (
        _float_number(value[0], f"{label}[0]"),
        _float_number(value[1], f"{label}[1]"),
    )


def _integer(value: object, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise UiAssetError(f"{label} must be an integer")
    return value


def _validate_integer_array(
    value: object,
    count: int,
    label: str,
    colour: bool = False,
    unsigned: bool = False,
) -> None:
    if not isinstance(value, list) or len(value) != count:
        raise UiAssetError(f"{label} must be a {count}-item array")
    for index, item in enumerate(value):
        number = _integer(item, f"{label}[{index}]")
        if colour and (number < 0 or number > 255):
            raise UiAssetError(f"{label}[{index}] must be between 0 and 255")
        if unsigned and (number < 0 or number > UINT32_MAX):
            raise UiAssetError(f"{label}[{index}] must be an unsigned 32-bit integer")
        if not colour and not unsigned and (number < INT32_MIN or number > INT32_MAX):
            raise UiAssetError(f"{label}[{index}] must be a signed 32-bit integer")


def _validate_property_type(
    value: object,
    value_type: str,
    label: str,
    nullable: bool,
) -> None:
    if value is None and nullable:
        return
    if value_type == "bool":
        if not isinstance(value, bool):
            raise UiAssetError(f"{label} must be a boolean")
    elif value_type == "int":
        number = _integer(value, label)
        if number < INT32_MIN or number > INT32_MAX:
            raise UiAssetError(f"{label} must be a signed 32-bit integer")
    elif value_type == "float":
        _float_number(value, label)
    elif value_type == "string":
        if not isinstance(value, str):
            raise UiAssetError(f"{label} must be a string")
    elif value_type == "sf.Text.LineAlignment":
        if not isinstance(value, str) or value not in {
            "default",
            "left",
            "center",
            "right",
        }:
            raise UiAssetError(f"{label} must be default, left, center or right")
    elif value_type == "Engine.TextGradientDirection":
        if not isinstance(value, str) or value not in {"vertical", "horizontal"}:
            raise UiAssetError(f"{label} must be vertical or horizontal")
    elif value_type == "Engine.ImageDrawAs":
        if not isinstance(value, str) or value not in {"Image", "Tile"}:
            raise UiAssetError(
                f"{label} must name an Engine.ImageDrawAs member: Image or Tile"
            )
    elif value_type == "string[]":
        if not isinstance(value, list) or any(
            not isinstance(item, str) for item in value
        ):
            raise UiAssetError(f"{label} must be a string array")
    elif value_type == "sf.Vector2f":
        _pair(value, label)
    elif value_type == "sf.Vector2u":
        _validate_integer_array(value, 2, label, unsigned=True)
    elif value_type == "sf.IntRect":
        _validate_integer_array(value, 4, label)
    elif value_type == "sf.Color":
        _validate_integer_array(value, 4, label, colour=True)
    else:
        raise UiAssetError(f"{label} uses unsupported property type {value_type}")
