# -*- encoding: utf-8 -*-
from __future__ import annotations

import os
import sys
import sysconfig
import tomllib

BINDGEN_DIR = os.path.dirname(os.path.abspath(__file__))
C_EXT_DIR = os.path.dirname(BINDGEN_DIR)
sys.path.insert(0, C_EXT_DIR)

from bindgen.parser import HeaderParser, ParsedHeader
from bindgen.codegen import BindingCodegen, CMakeGen


def load_config() -> dict:
    config_path = os.path.join(C_EXT_DIR, "extensions.toml")
    with open(config_path, "rb") as f:
        return tomllib.load(f)


def collect_source_files(ext_dir: str, src_dir: str = "src") -> list[str]:
    source_files = []
    src_path = os.path.join(ext_dir, src_dir)
    if not os.path.exists(src_path):
        return source_files
    for root, _dirs, files in os.walk(src_path):
        for f in files:
            if f.endswith((".cpp", ".c", ".cxx")):
                rel = os.path.relpath(os.path.join(root, f), ext_dir)
                source_files.append(rel.replace("\\", "/"))
    return source_files


def collect_headers(ext_dir: str, include_dir: str = "include") -> list[str]:
    headers = []
    inc_path = os.path.join(ext_dir, include_dir)
    if not os.path.exists(inc_path):
        return headers
    for root, _dirs, files in os.walk(inc_path):
        for f in files:
            if f.endswith((".hpp", ".h", ".hxx")):
                headers.append(os.path.join(root, f))
    return headers


def generate_extension(ext_key: str, ext_config: dict) -> bool:
    name = ext_config["name"]
    ext_dir = os.path.join(C_EXT_DIR, ext_config.get("source_dir", name.replace("Ext", "Ext")))

    if not os.path.isdir(ext_dir):
        possible_dirs = [
            os.path.join(C_EXT_DIR, name),
            os.path.join(C_EXT_DIR, ext_key),
        ]
        ext_dir = None
        for d in possible_dirs:
            if os.path.isdir(d):
                ext_dir = d
                break
        if ext_dir is None:
            print(f"[SKIP] Cannot find source directory for {name}")
            return False

    print(f"[BINDGEN] Processing {name} at {ext_dir}")

    bindgen_conf = ext_config.get("bindgen", {})
    needs_sfml = ext_config.get("needs_pysf", False)
    internals_id = bindgen_conf.get("internals_id", "PYSF")
    pysf_import = bindgen_conf.get("pysf_import", None)
    extra_includes = bindgen_conf.get("extra_includes", [])
    extra_args = bindgen_conf.get("clang_args", [])

    include_dirs = [os.path.join(ext_dir, "include"), BINDGEN_DIR]

    sfml_include = os.path.join(C_EXT_DIR, "SFML", "include")
    if os.path.exists(sfml_include):
        include_dirs.append(sfml_include)

    try:
        import pybind11
        include_dirs.append(pybind11.get_include())
    except ImportError:
        pass

    python_include = sysconfig.get_path("include")
    if python_include and os.path.isdir(python_include):
        include_dirs.append(python_include)

    for inc in bindgen_conf.get("include_paths", []):
        resolved = os.path.join(C_EXT_DIR, inc)
        if os.path.isdir(resolved):
            include_dirs.append(resolved)

    parser = HeaderParser(include_paths=include_dirs, extra_args=extra_args)
    headers = collect_headers(ext_dir)
    parsed_headers: list[ParsedHeader] = []

    for header_path in headers:
        parsed = parser.parse(header_path)
        if parsed.classes or parsed.functions:
            parsed_headers.append(parsed)
            print(f"  [OK] {os.path.relpath(header_path, ext_dir)}: "
                  f"{len(parsed.classes)} classes, {len(parsed.functions)} functions")
        else:
            print(f"  [--] {os.path.relpath(header_path, ext_dir)}: no bindings found")

    if not parsed_headers:
        print(f"[SKIP] No bindable declarations found in {name}")
        return False

    codegen = BindingCodegen(module_name=name, internals_id=internals_id)

    formatted_includes = []
    for inc in extra_includes:
        if not inc.startswith("<") and not inc.startswith('"'):
            formatted_includes.append(f'"{inc}"')
        else:
            formatted_includes.append(inc)

    binding_code = codegen.generate(
        headers=parsed_headers,
        pysf_import=pysf_import,
        extra_includes=formatted_includes,
        include_base_dir=os.path.join(ext_dir, "include"),
    )

    generated_path = os.path.join(ext_dir, "_generated_bindings.cpp")
    with open(generated_path, "w", encoding="utf-8") as f:
        f.write(binding_code)
    print(f"  [GEN] {os.path.relpath(generated_path, C_EXT_DIR)}")

    source_files = collect_source_files(ext_dir)
    cmake_gen = CMakeGen(project_name=name)
    cmake_content = cmake_gen.generate(
        source_files=source_files,
        generated_binding="_generated_bindings.cpp",
        needs_sfml=needs_sfml,
    )

    cmake_path = os.path.join(ext_dir, "CMakeLists.txt")
    with open(cmake_path, "w", encoding="utf-8") as f:
        f.write(cmake_content)
    print(f"  [GEN] {os.path.relpath(cmake_path, C_EXT_DIR)}")

    return True


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Ludork Bindgen")
    parser.add_argument("--only", type=str, default=None)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    config = load_config()
    success_count = 0
    total_count = 0

    for key, ext_config in config.items():
        if not isinstance(ext_config, dict) or "name" not in ext_config:
            continue

        if args.only and ext_config["name"] != args.only:
            continue

        if not ext_config.get("bindgen", {}).get("enabled", False):
            print(f"[SKIP] {ext_config['name']}: bindgen not enabled")
            continue

        total_count += 1
        if not args.dry_run:
            if generate_extension(key, ext_config):
                success_count += 1
        else:
            print(f"[DRY-RUN] Would generate bindings for {ext_config['name']}")
            success_count += 1

    print(f"\n[DONE] Generated {success_count}/{total_count} extensions")
    return 0 if success_count == total_count else 1


if __name__ == "__main__":
    sys.exit(main())
