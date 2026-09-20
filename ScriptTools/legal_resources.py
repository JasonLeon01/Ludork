from __future__ import annotations

import argparse
import pathlib


def copy_markdown(
    source: pathlib.Path,
    target: pathlib.Path,
    links: dict[str, str],
) -> None:
    if source.resolve() == target.resolve():
        raise ValueError(f"Legal resource output must differ from its source: {source}")
    text = source.read_bytes().decode("utf-8")
    for original, relocated in links.items():
        text = text.replace(f"]({original})", f"]({relocated})")
    content = text.encode("utf-8")
    if not target.is_file() or target.read_bytes() != content:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)


def copy_resources(source_root: pathlib.Path, output_root: pathlib.Path, mode: str) -> None:
    if mode == "editor":
        copy_markdown(source_root / "LICENSE.md", output_root / "LICENSE.md", {})
    for suffix in ("", "_zh_CN"):
        notice_name = f"THIRD_PARTY_NOTICES{suffix}.md"
        readme_name = f"README{suffix}.md"
        if mode == "editor":
            copy_markdown(
                source_root / readme_name,
                output_root / readme_name,
                {},
            )
            copy_markdown(
                source_root / "docs" / notice_name,
                output_root / "docs" / notice_name,
                {},
            )
        copy_markdown(
            source_root / "Licenses" / readme_name,
            output_root / "Licenses" / readme_name,
            {f"../docs/{notice_name}": f"../{notice_name}"} if mode == "template-index" else {},
        )


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools legal-resources")
    parser.add_argument("mode", choices=("editor", "template-index"))
    parser.add_argument("source_root", type=pathlib.Path)
    parser.add_argument("output_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    copy_resources(parsed.source_root.resolve(), parsed.output_root.resolve(), parsed.mode)
    return 0
