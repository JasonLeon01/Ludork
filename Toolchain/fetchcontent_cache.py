from __future__ import annotations

import pathlib


_DEPENDENCY_NAMES = (
    "flac", "freetype", "harfbuzz", "libssh2", "ludork_utf8proc", "mbedtls", "ogg", "sheenbidi", "vorbis",
)
_COMMON_CACHE_DIRECTORIES = ("build/_deps", "build/Release/_deps", "build/Debug/_deps")


def cached_dependency_sources(
    project_dir: pathlib.Path,
    *,
    preferred_roots: tuple[pathlib.Path, ...] = (),
) -> dict[str, pathlib.Path]:
    roots = (*preferred_roots, *(project_dir / relative for relative in _COMMON_CACHE_DIRECTORIES))
    sources: dict[str, pathlib.Path] = {}
    for name in _DEPENDENCY_NAMES:
        for root in roots:
            source = root / f"{name}-src"
            if (source / "CMakeLists.txt").is_file():
                sources[name.upper()] = source
                break
    return sources
