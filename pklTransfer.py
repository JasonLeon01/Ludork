# -*- encoding: utf-8 -*-

import json
import pickle
import sys
import os


def json2pkl(json_path, ext=None):
    if not os.path.isfile(json_path):
        raise FileNotFoundError(f"JSON file not found: {json_path}")

    base, _ = os.path.splitext(json_path)
    pkl_path = base + (ext if ext else ".pkl")

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    with open(pkl_path, "wb") as f:
        pickle.dump(data, f)

    print(f"Converted {json_path} -> {pkl_path}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python pklTransfer.py <path_to_json_file>")
        sys.exit(1)
    if len(sys.argv) == 2:
        json2pkl(sys.argv[1])
    else:
        json2pkl(sys.argv[1], sys.argv[2])
