# -*- encoding: utf-8 -*-

import json
import pickle
import sys
import os


def json2localePickles(json_path: str) -> None:
    if not os.path.isfile(json_path):
        raise FileNotFoundError(f"JSON file not found: {json_path}")

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    lang_buckets = {}
    for key, value in data.items():
        if not isinstance(value, dict):
            continue
        for lang, content in value.items():
            if lang is None:
                continue
            if lang not in lang_buckets:
                lang_buckets[lang] = {}
            lang_buckets[lang][key] = content

    out_dir = os.path.dirname(json_path)
    for lang, mapping in lang_buckets.items():
        out_path = os.path.join(out_dir, str(lang))
        with open(out_path, "wb") as f:
            pickle.dump(mapping, f)
        print(f"Converted {json_path} -> {out_path} ({len(mapping)} entries)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python localeTransfer.py <path_to_json_file>")
        sys.exit(1)
    json2localePickles(sys.argv[1])
