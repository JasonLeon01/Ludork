from __future__ import annotations

import math
import struct

from . import runtime_formats as formats


def encode_data(value: object) -> bytes:
    output = bytearray()

    def append(data: bytes) -> None:
        if len(data) > formats.MAX_DATA_SIZE - len(output):
            raise ValueError("Binary data exceeds the maximum payload size")
        output.extend(data)

    def length(value: int) -> None:
        if value > 0xFFFFFFFF:
            raise ValueError("Binary data length exceeds uint32")
        append(struct.pack("<I", value))

    def string(data: bytes) -> None:
        length(len(data))
        append(data)

    def encode(item: object, depth: int) -> None:
        if depth > formats.DATA_MAXIMUM_DEPTH:
            raise ValueError("Binary data exceeds the maximum nesting depth")
        if item is None:
            append(bytes((formats.DATA_NULL_TAG,)))
        elif isinstance(item, bool):
            append(bytes((formats.DATA_TRUE_TAG if item else formats.DATA_FALSE_TAG,)))
        elif isinstance(item, int):
            if not -(1 << 63) <= item < (1 << 63):
                raise ValueError("Binary data integer is outside int64 range")
            append(struct.pack("<Bq", formats.DATA_INTEGER_TAG, item))
        elif isinstance(item, float):
            if not math.isfinite(item):
                raise ValueError("Binary data floating-point value must be finite")
            append(struct.pack("<Bd", formats.DATA_FLOAT_TAG, item))
        elif isinstance(item, str):
            append(bytes((formats.DATA_STRING_TAG,)))
            string(item.encode("utf-8"))
        elif isinstance(item, list):
            append(bytes((formats.DATA_ARRAY_TAG,)))
            length(len(item))
            for child in item:
                encode(child, depth + 1)
        elif isinstance(item, dict):
            append(bytes((formats.DATA_MAP_TAG,)))
            length(len(item))
            entries = []
            for key, child in item.items():
                if not isinstance(key, str):
                    raise ValueError("Binary data map keys must be strings")
                entries.append((key.encode("utf-8"), child))
            for key, child in sorted(entries, key=lambda entry: entry[0]):
                string(key)
                encode(child, depth + 1)
        else:
            raise ValueError(f"Unsupported binary data value: {type(item).__name__}")

    encode(value, 0)
    return bytes(output)
