# -*- encoding: utf-8 -*-
from __future__ import annotations

import os
import re
import sys
from dataclasses import dataclass
from typing import Optional


@dataclass
class BoundMethod:
    class_name: str
    method_name: str
    has_return_policy: bool = False
    return_policy: str = ""


@dataclass
class BoundProperty:
    class_name: str
    prop_name: str
    is_readonly: bool = False


@dataclass
class BoundFunction:
    func_name: str


@dataclass
class BoundClass:
    class_name: str
    bind_name: str = ""
    has_init: bool = False


def parse_existing_bindings(binding_cpp_path: str) -> dict:
    with open(binding_cpp_path, "r", encoding="utf-8") as f:
        content = f.read()

    result = {
        "classes": [],
        "methods": [],
        "properties": [],
        "functions": [],
    }

    class_re = re.compile(r'py::class_<(\w+)[^>]*>\s+\w+\s*\(\s*m\s*,\s*"(\w+)"')
    for match in class_re.finditer(content):
        result["classes"].append(
            BoundClass(class_name=match.group(1), bind_name=match.group(2))
        )

    init_re = re.compile(r'(\w+)\.def\(py::init<([^>]*)>')
    for match in init_re.finditer(content):
        for cls in result["classes"]:
            if cls.class_name in match.group(0):
                cls.has_init = True

    method_re = re.compile(
        r'\w+\.def\(\s*"(\w+)"\s*,\s*&(\w+)::(\w+)'
        r'(?:.*?(py::return_value_policy::(\w+)))?'
    )
    for match in method_re.finditer(content):
        result["methods"].append(
            BoundMethod(
                class_name=match.group(2),
                method_name=match.group(3),
                has_return_policy=match.group(4) is not None,
                return_policy=match.group(5) or "",
            )
        )

    prop_re = re.compile(
        r'\w+\.def_(readwrite|readonly)\(\s*"(\w+)"\s*,\s*&(\w+)::(\w+)'
    )
    for match in prop_re.finditer(content):
        result["properties"].append(
            BoundProperty(
                class_name=match.group(3),
                prop_name=match.group(4),
                is_readonly=(match.group(1) == "readonly"),
            )
        )

    func_re = re.compile(r'm\.def\(\s*"(\w+)"\s*,\s*&(\w+)')
    for match in func_re.finditer(content):
        result["functions"].append(BoundFunction(func_name=match.group(2)))

    lambda_func_re = re.compile(r'm\.def\(\s*"(\w+)"\s*,\s*\[')
    for match in lambda_func_re.finditer(content):
        result["functions"].append(BoundFunction(func_name=match.group(1)))

    return result


def annotate_header(header_path: str, bindings: dict) -> str:
    with open(header_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    bound_classes = {c.class_name for c in bindings["classes"]}
    bound_methods = {(m.class_name, m.method_name): m for m in bindings["methods"]}
    bound_props = {(p.class_name, p.prop_name): p for p in bindings["properties"]}
    bound_funcs = {f.func_name for f in bindings["functions"]}

    output_lines = []
    current_class = None
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.rstrip()

        class_match = re.match(r'\s*(class|struct)\s+(\w+)\s*(?::|{|$)', stripped)
        if class_match:
            class_name = class_match.group(2)
            if class_name in bound_classes:
                if i > 0 and "BIND_" not in (output_lines[-1] if output_lines else ""):
                    output_lines.append(f"// BIND_CLASS\n")
                current_class = class_name

        if current_class is None:
            func_match = re.match(r'\s*(?:[\w:]+\s+)*(\w+)\s*\(', stripped)
            if func_match and not stripped.strip().startswith("//"):
                func_name = func_match.group(1)
                if func_name in bound_funcs:
                    if not output_lines or "BIND_" not in output_lines[-1]:
                        output_lines.append(f"// BIND_FUNCTION\n")

        if current_class:
            method_match = re.match(
                r'\s*(?:virtual\s+)?(?:[\w:<>,\s\*&]+?)\s+(\w+)\s*\(', stripped
            )
            if method_match and not stripped.strip().startswith("//"):
                method_name = method_match.group(1)
                key = (current_class, method_name)
                if key in bound_methods:
                    bm = bound_methods[key]
                    if not output_lines or "BIND_" not in output_lines[-1]:
                        if bm.has_return_policy:
                            output_lines.append(
                                f'// BIND_METHOD(return_policy="{bm.return_policy}")\n'
                            )
                        else:
                            output_lines.append(f"// BIND_METHOD\n")

            field_match = re.match(r'\s*(?:[\w:<>,\s]+?)\s+(\w+)\s*;', stripped)
            if field_match and not stripped.strip().startswith("//"):
                field_name = field_match.group(1)
                key = (current_class, field_name)
                if key in bound_props:
                    bp = bound_props[key]
                    if not output_lines or "BIND_" not in output_lines[-1]:
                        if bp.is_readonly:
                            output_lines.append(f"// BIND_PROPERTY(readonly=true)\n")
                        else:
                            output_lines.append(f"// BIND_PROPERTY\n")

        if current_class and stripped.strip() == "};":
            indent = len(line) - len(line.lstrip())
            if indent == 0:
                current_class = None

        output_lines.append(line)
        i += 1

    return "".join(output_lines)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Migrate existing pybind11 bindings to bindgen annotations")
    parser.add_argument("binding_cpp")
    parser.add_argument("headers", nargs="+")
    parser.add_argument("--output-dir", default=None)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    print(f"[MIGRATE] Parsing bindings from: {args.binding_cpp}")
    bindings = parse_existing_bindings(args.binding_cpp)
    print(f"  Found: {len(bindings['classes'])} classes, "
          f"{len(bindings['methods'])} methods, "
          f"{len(bindings['properties'])} properties, "
          f"{len(bindings['functions'])} functions")

    for header_path in args.headers:
        if not os.path.exists(header_path):
            print(f"  [SKIP] {header_path}: not found")
            continue

        print(f"  [ANNOTATE] {header_path}")
        annotated = annotate_header(header_path, bindings)

        if args.dry_run:
            print(annotated)
        else:
            out_path = header_path
            if args.output_dir:
                os.makedirs(args.output_dir, exist_ok=True)
                out_path = os.path.join(args.output_dir, os.path.basename(header_path))
            with open(out_path, "w", encoding="utf-8") as f:
                f.write(annotated)
            print(f"    -> {out_path}")

    print("[DONE]")


if __name__ == "__main__":
    main()
