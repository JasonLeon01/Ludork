from __future__ import annotations

import pathlib

from .finalize_package import finalize_staged_package, resolve_finalization_inputs
from .ldpak import validate_ldpak_source
from .package_contents import copy_package_inputs
from .package_staging import package_staging
from .packaging_metadata import PackageMetadata


def prepare_runtime_resources(
    project: pathlib.Path,
    destination: pathlib.Path,
    *,
    metadata: PackageMetadata,
    registry: pathlib.Path,
    licenses: bool,
    compile_lua: bool,
    encrypt_shaders: bool,
    encrypt_data: bool,
    use_ldpak: bool,
) -> None:
    if registry is None:
        raise TypeError("A prepared UI registry path is required for package resources.")
    if use_ldpak:
        validate_ldpak_source(project)
    with package_staging(destination) as working_root:
        copy_package_inputs(project, working_root, licenses=licenses)
        metadata.write_build_info(working_root)
        inputs = resolve_finalization_inputs(working_root, registry=registry)
        finalize_staged_package(
            working_root,
            inputs,
            encrypt_shaders_enabled=encrypt_shaders,
            encrypt_data_enabled=encrypt_data,
            compile_lua_enabled=compile_lua,
            use_ldpak=use_ldpak,
        )
