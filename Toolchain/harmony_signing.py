from __future__ import annotations

import argparse
import json
import math
import os
import pathlib
import re
import ssl
import subprocess
import tempfile
import time
from dataclasses import dataclass
from typing import TextIO

from .pack_error import PackError
from .packaging_constants import EXIT_SIGNING, EXIT_TOOLCHAIN


@dataclass(frozen=True, repr=False)
class HarmonySigningOptions:
    keystore_path: pathlib.Path
    certificate_path: pathlib.Path
    profile_path: pathlib.Path
    key_alias: str
    keystore_password: str
    key_password: str

    def __repr__(self) -> str:
        return "HarmonySigningOptions(<redacted>)"

    def redact(self, message: str) -> str:
        values = (
            str(self.keystore_path), str(self.certificate_path), str(self.profile_path),
            self.key_alias, self.keystore_password, self.key_password,
        )
        for value in sorted(set(values), key=len, reverse=True):
            if value:
                message = message.replace(value, "[REDACTED]")
        return message


def read_signing_options(
    arguments: argparse.Namespace,
    input_stream: TextIO,
    project_dir: pathlib.Path,
) -> HarmonySigningOptions | None:
    values = (arguments.keystore, arguments.certificate, arguments.profile, arguments.key_alias)
    if not arguments.sign:
        if any(value is not None for value in values) or arguments.export_to_device:
            raise PackError("HarmonyOS signing arguments and device export require --sign.", EXIT_SIGNING)
        return None
    if any(value is None for value in values):
        raise PackError("HarmonyOS signing requires a keystore, certificate, profile and key alias.", EXIT_SIGNING)
    paths: list[pathlib.Path] = []
    for value in values[:3]:
        if not isinstance(value, pathlib.Path) or not value.is_absolute():
            raise PackError("HarmonyOS signing material paths must be absolute.", EXIT_SIGNING)
        try:
            path = value.resolve(strict=True)
        except (OSError, RuntimeError) as exception:
            raise PackError("A HarmonyOS signing material file is unavailable.", EXIT_SIGNING) from exception
        if not path.is_file() or not os.access(path, os.R_OK):
            raise PackError("A HarmonyOS signing material file is unavailable.", EXIT_SIGNING)
        if path.is_relative_to(project_dir) or any((parent / ".git").exists() for parent in path.parents):
            raise PackError("Keep HarmonyOS signing materials outside project and Git directories.", EXIT_SIGNING)
        paths.append(path)
    alias = arguments.key_alias
    if not alias.strip() or "\r" in alias or "\n" in alias:
        raise PackError("The HarmonyOS signing key alias is invalid.", EXIT_SIGNING)
    if input_stream.isatty():
        raise PackError("Supply two HarmonyOS signing password lines on standard input.", EXIT_SIGNING)
    try:
        passwords = input_stream.read().removesuffix("\n").split("\n")
    except (OSError, UnicodeError) as exception:
        raise PackError("Unable to read HarmonyOS signing passwords.", EXIT_SIGNING) from exception
    if len(passwords) != 2 or any(not value or "\r" in value for value in passwords):
        raise PackError("HarmonyOS signing passwords must be two non-empty UTF-8 lines.", EXIT_SIGNING)
    return HarmonySigningOptions(*paths, alias, *passwords)


def require_signing_tools(java_home: pathlib.Path, sign_tool: pathlib.Path) -> None:
    if not sign_tool.is_file() or not (java_home / "bin" / "java").is_file():
        raise PackError("DevEco Studio HAP signing tools are unavailable.", EXIT_TOOLCHAIN)


def signing_workspace(prefix: str) -> tempfile.TemporaryDirectory[str]:
    root = pathlib.Path("/private/tmp").resolve()
    if any((directory / ".git").exists() for directory in (root, *root.parents)):
        raise PackError("HarmonyOS signing requires a temporary directory outside Git.", EXIT_SIGNING)
    return tempfile.TemporaryDirectory(prefix=prefix, dir=root)


def run_signing_tool(
    command: list[str],
    directory: pathlib.Path,
    signing: HarmonySigningOptions | None = None,
    *,
    input_text: str | None = None,
    environment: dict[str, str] | None = None,
    timeout: float = 30.0,
) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(
            command, cwd=directory, input=input_text, env=environment,
            capture_output=True, encoding="utf-8", errors="replace", timeout=timeout, check=False,
        )
    except (OSError, subprocess.TimeoutExpired, ValueError) as exception:
        # Exception text can include the full command and its passwords.
        raise PackError("Unable to run the HarmonyOS signing tool.", EXIT_SIGNING) from exception
    if result.returncode != 0:
        message = "HarmonyOS signing tool failed."
        if signing is not None:
            diagnostic = signing.redact(result.stdout + "\n" + result.stderr).strip()
            if diagnostic:
                message += "\n" + diagnostic[-4096:]
        raise PackError(message, EXIT_SIGNING)
    return result


def verify_profile(java_home: pathlib.Path, sign_tool: pathlib.Path, profile: pathlib.Path) -> dict[str, object]:
    with signing_workspace("ludork-harmony-profile-") as temporary:
        directory = pathlib.Path(temporary)
        output = directory / "verification.json"
        run_signing_tool([
            str(java_home / "bin" / "java"), "-jar", str(sign_tool), "verify-profile",
            "-inFile", str(profile), "-outFile", str(output),
        ], directory)
        try:
            value = json.loads(output.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exception:
            raise PackError("Unable to verify the HarmonyOS profile.", EXIT_SIGNING) from exception
    if not isinstance(value, dict) or value.get("verifiedPassed") is not True or not isinstance(value.get("content"), dict):
        raise PackError("The HarmonyOS profile signature is invalid.", EXIT_SIGNING)
    return value["content"]


def validate_profile(content: dict[str, object], bundle_name: str, device_udid: str | None = None) -> None:
    kind = content.get("type")
    info = content.get("bundle-info")
    validity = content.get("validity")
    if kind not in {"debug", "release"} or not isinstance(info, dict) or info.get("bundle-name") != bundle_name:
        raise PackError("The HarmonyOS profile type or bundle name does not match the app.", EXIT_SIGNING)
    if not isinstance(validity, dict):
        raise PackError("The HarmonyOS profile validity is missing.", EXIT_SIGNING)
    start, end = validity.get("not-before"), validity.get("not-after")
    if any(isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) for value in (start, end)):
        raise PackError("The HarmonyOS profile validity is invalid.", EXIT_SIGNING)
    if not start <= time.time() <= end:
        raise PackError("The HarmonyOS profile has expired or is not yet valid.", EXIT_SIGNING)
    if device_udid is not None and kind == "debug":
        debug = content.get("debug-info")
        if not isinstance(debug, dict) or debug.get("device-id-type") != "udid" or not isinstance(debug.get("device-ids"), list) or device_udid not in debug["device-ids"]:
            raise PackError("The HarmonyOS debug profile does not authorize the selected device.", EXIT_SIGNING)


def certificate_der(value: str) -> bytes:
    match = re.search(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", value, re.DOTALL)
    if match is None:
        raise PackError("The HarmonyOS signing certificate is not a PEM certificate.", EXIT_SIGNING)
    try:
        return ssl.PEM_cert_to_DER_cert(match[0])
    except ValueError as exception:
        raise PackError("The HarmonyOS signing certificate is invalid.", EXIT_SIGNING) from exception


def validate_signing_credentials(
    java_home: pathlib.Path,
    sign_tool: pathlib.Path,
    signing: HarmonySigningOptions,
    bundle_name: str,
    device_udid: str | None = None,
) -> None:
    require_signing_tools(java_home, sign_tool)
    content = verify_profile(java_home, sign_tool, signing.profile_path)
    validate_profile(content, bundle_name, device_udid)
    certificate = signing.certificate_path.read_text(encoding="utf-8")
    profile_certificate = content["bundle-info"].get(
        "development-certificate" if content["type"] == "debug" else "distribution-certificate"
    )
    certificate_chain = re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", certificate, re.DOTALL)
    if not isinstance(profile_certificate, str) or certificate_der(profile_certificate) not in [certificate_der(item) for item in certificate_chain]:
        raise PackError("The signing certificate does not match the HarmonyOS profile.", EXIT_SIGNING)
    with signing_workspace("ludork-harmony-credentials-") as temporary:
        directory = pathlib.Path(temporary)
        request_path = directory / "check.csr"
        run_signing_tool([
            str(java_home / "bin" / "java"), "-jar", str(sign_tool), "generate-csr",
            "-keystoreFile", str(signing.keystore_path), "-keyAlias", signing.key_alias,
            "-signAlg", "SHA256withECDSA", "-subject", "CN=Ludork Signing Check",
            "-keystorePwd", signing.keystore_password, "-keyPwd", signing.key_password,
            "-outFile", str(request_path),
        ], directory, signing)
        request = request_path.read_text(encoding="utf-8")
        public_key = run_signing_tool([
            "/usr/bin/openssl", "req", "-pubkey", "-noout",
        ], directory, signing, input_text=request).stdout.strip()
        certificate_key = run_signing_tool([
            "/usr/bin/openssl", "x509", "-pubkey", "-noout",
        ], directory, signing, input_text=profile_certificate).stdout.strip()
        run_signing_tool([
            "/usr/bin/openssl", "x509", "-checkend", "0", "-noout",
        ], directory, signing, input_text=profile_certificate)
        if not public_key or public_key != certificate_key:
            raise PackError("The HarmonyOS private key does not match the signing certificate.", EXIT_SIGNING)


def sign_hap(
    java_home: pathlib.Path,
    sign_tool: pathlib.Path,
    signing: HarmonySigningOptions,
    unsigned_hap: pathlib.Path,
    output: pathlib.Path,
    compatible_api: int,
) -> None:
    run_signing_tool([
        str(java_home / "bin" / "java"), "-jar", str(sign_tool), "sign-app",
        "-mode", "localSign", "-keyAlias", signing.key_alias, "-signAlg", "SHA256withECDSA",
        "-appCertFile", str(signing.certificate_path), "-profileFile", str(signing.profile_path),
        "-inFile", str(unsigned_hap), "-keystoreFile", str(signing.keystore_path),
        "-outFile", str(output), "-keyPwd", signing.key_password, "-keystorePwd", signing.keystore_password,
        "-compatibleVersion", str(compatible_api), "-signCode", "1",
    ], output.parent, signing, timeout=120.0)
    if not output.is_file():
        raise PackError("The HarmonyOS signing tool did not produce a signed HAP.", EXIT_SIGNING)


def verify_signed_hap(
    java_home: pathlib.Path,
    sign_tool: pathlib.Path,
    hap: pathlib.Path,
    signing: HarmonySigningOptions,
    bundle_name: str,
    device_udid: str | None = None,
) -> None:
    with signing_workspace("ludork-harmony-verify-") as temporary:
        directory = pathlib.Path(temporary)
        certificate, profile = directory / "certificate.cer", directory / "profile.p7b"
        run_signing_tool([
            str(java_home / "bin" / "java"), "-jar", str(sign_tool), "verify-app",
            "-inFile", str(hap), "-outCertChain", str(certificate), "-outProfile", str(profile),
        ], directory, signing)
        if not certificate.is_file() or not profile.is_file():
            raise PackError("The signed HarmonyOS HAP has an invalid signature.", EXIT_SIGNING)
        if profile.read_bytes() != signing.profile_path.read_bytes():
            raise PackError("The signed HAP does not contain the selected HarmonyOS profile.", EXIT_SIGNING)
        validate_profile(verify_profile(java_home, sign_tool, profile), bundle_name, device_udid)
