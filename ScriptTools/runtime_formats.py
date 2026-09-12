from __future__ import annotations

import struct


LDPK_MAGIC = b"LDPK"
LDPK_VERSION = 1
LDPK_FLAGS = 0
LDPK_DIRECTORY_FLAG = 1
LDPK_ALIGNMENT = 8
LDPK_HEADER_FIELDS = (
    ("Magic", "4s"), ("Version", "H"), ("Flags", "H"),
    ("GroupLength", "I"), ("EntryCount", "I"), ("IndexOffset", "Q"),
    ("IndexSize", "Q"), ("IndexCrc", "I"), ("Reserved", "I"),
)
LDPK_ENTRY_FIELDS = (
    ("PathLength", "I"), ("Flags", "I"), ("DataOffset", "Q"),
    ("DataSize", "Q"), ("DataCrc", "I"), ("Reserved", "I"),
)
LDPK_HEADER = struct.Struct("<" + "".join(code for _, code in LDPK_HEADER_FIELDS))
LDPK_ENTRY = struct.Struct("<" + "".join(code for _, code in LDPK_ENTRY_FIELDS))

SHADER_MAGIC = b"LDSC"
DATA_MAGIC = b"LDDC"
ENCRYPTED_VERSION = 1
ENCRYPTED_ZLIB_FLAG = 1
ENCRYPTED_HEADER_FIELDS = (
    ("Magic", "4s"), ("Version", "B"), ("Flags", "B"),
    ("Reserved", "H"), ("SourceSize", "I"), ("Checksum", "I"), ("Nonce", "Q"),
)
ENCRYPTED_HEADER = struct.Struct("<" + "".join(code for _, code in ENCRYPTED_HEADER_FIELDS))
KEY_SEED = 0xD6E8FEB86659FD93
STREAM_MULTIPLIER = 0x2545F4914F6CDD1D
STREAM_FALLBACK = 0x9E3779B97F4A7C15
STREAM_RIGHT_SHIFT_FIRST = 12
STREAM_LEFT_SHIFT = 25
STREAM_RIGHT_SHIFT_LAST = 27
STREAM_BLOCK_SIZE = struct.calcsize("<Q")
MAX_SHADER_SIZE = 64 * 1024 * 1024
MAX_DATA_SIZE = 512 * 1024 * 1024
