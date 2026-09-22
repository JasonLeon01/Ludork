from __future__ import annotations

import argparse
import os
import pathlib
import sys
from dataclasses import dataclass

from .apple_signing import (
    AD_HOC_IDENTITY,
    MACOS_SIGNING_CERTIFICATE_ENVIRONMENT,
    MACOS_SIGNING_ENTITLEMENTS_ENVIRONMENT,
    MACOS_SIGNING_IDENTITY_ENVIRONMENT,
    NotaryCredentials,
    OptionSource,
    TemporaryKeychain,
    absolute_path,
    notarize_and_staple,
    read_secret_lines,
    require_macos,
    resolve_notary_credentials,
    select_identity,
    sign_artifact,
    store_notary_credentials,
)
from .pack_error import PackError
from .packaging_constants import EXIT_SIGNING


@dataclass(frozen=True)
class SigningSession:
    target: pathlib.Path
    requested_identity: str
    certificate: pathlib.Path | None
    certificate_password: str
    entitlements: pathlib.Path | None
    credentials: NotaryCredentials | None
    notary_password: str
    check_only: bool
    environment: dict[str, str]


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ScriptTools macos-sign",
        usage=(
            "macos-sign [--signing-identity NAME] [--certificate PATH.p12] "
            "[--entitlements PATH.plist] [--notarize] [--notary-apple-id EMAIL] "
            "[--notary-team-id TEAMID] [--notary-key PATH.p8] [--notary-key-id ID] "
            "[--notary-key-issuer UUID] [--check] <app-or-dmg>"
        ),
    )
    parser.add_argument("target", type=pathlib.Path)
    parser.add_argument("--signing-identity")
    parser.add_argument("--certificate", type=pathlib.Path)
    parser.add_argument("--entitlements", type=pathlib.Path)
    parser.add_argument("--notarize", action="store_true")
    parser.add_argument("--notary-apple-id")
    parser.add_argument("--notary-team-id")
    parser.add_argument("--notary-key", type=pathlib.Path)
    parser.add_argument("--notary-key-id")
    parser.add_argument("--notary-key-issuer")
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--ignore-environment",
        action="store_true",
        help="Ignore the LUDORK_* signing environment variables and use only the command-line options",
    )
    return parser


def _optional_path(value: str, description: str, environment: str) -> pathlib.Path | None:
    if not value:
        return None
    return absolute_path(value, description, environment)


def create_session(parsed: argparse.Namespace) -> SigningSession:
    target = parsed.target.expanduser().resolve()
    if not parsed.check and not target.exists():
        raise PackError(f"The signing target was not found: {target}", EXIT_SIGNING)
    source = OptionSource(parsed.ignore_environment)
    certificate = _optional_path(
        source.value(
            MACOS_SIGNING_CERTIFICATE_ENVIRONMENT,
            str(parsed.certificate) if parsed.certificate is not None else None,
        ),
        "The signing certificate",
        MACOS_SIGNING_CERTIFICATE_ENVIRONMENT,
    )
    entitlements = _optional_path(
        source.value(
            MACOS_SIGNING_ENTITLEMENTS_ENVIRONMENT,
            str(parsed.entitlements) if parsed.entitlements is not None else None,
        ),
        "The signing entitlements",
        MACOS_SIGNING_ENTITLEMENTS_ENVIRONMENT,
    )
    credentials = (
        resolve_notary_credentials(
            source,
            apple_id=parsed.notary_apple_id,
            team_id=parsed.notary_team_id,
            key=parsed.notary_key,
            key_id=parsed.notary_key_id,
            key_issuer=parsed.notary_key_issuer,
        )
        if parsed.notarize
        else None
    )
    secrets = read_secret_lines(
        sys.stdin,
        (1 if certificate is not None else 0)
        + (1 if credentials is not None and credentials.password_required else 0),
        "macOS signing",
    )
    position = 0
    certificate_password = ""
    if certificate is not None:
        certificate_password = secrets[position]
        position += 1
    notary_password = ""
    if credentials is not None and credentials.password_required:
        notary_password = secrets[position]
    return SigningSession(
        target,
        source.value(MACOS_SIGNING_IDENTITY_ENVIRONMENT, parsed.signing_identity),
        certificate,
        certificate_password,
        entitlements,
        credentials,
        notary_password,
        parsed.check,
        os.environ.copy(),
    )


def _keychain_required(session: SigningSession) -> bool:
    if session.certificate is not None:
        return True
    return session.credentials is not None and session.credentials.password_required


def _describe(session: SigningSession, identity: str) -> None:
    print(f"Signing target: {session.target}", flush=True)
    print(f"Signing identity: {identity}", flush=True)
    if session.entitlements is not None:
        print(f"Signing entitlements: {session.entitlements}", flush=True)
    if session.credentials is not None:
        mode = "Apple ID" if session.credentials.apple_id else "App Store Connect key"
        print(f"Notarisation credentials: {mode}", flush=True)


def perform(session: SigningSession, keychain: TemporaryKeychain | None) -> None:
    identity = AD_HOC_IDENTITY
    if session.certificate is not None:
        if keychain is None:
            raise PackError("The signing keychain is unavailable.", EXIT_SIGNING)
        keychain.add_certificate(session.certificate, session.certificate_password)
        identity = select_identity(keychain.identities(), session.requested_identity).identifier
    elif session.requested_identity:
        identity = session.requested_identity
    if session.credentials is not None and identity == AD_HOC_IDENTITY and not session.check_only:
        raise PackError(
            "Notarisation requires a Developer ID signing identity.",
            EXIT_SIGNING,
        )
    if session.credentials is not None and session.credentials.password_required:
        if keychain is None:
            raise PackError("The notarisation keychain is unavailable.", EXIT_SIGNING)
        store_notary_credentials(keychain, session.credentials, session.notary_password)
    _describe(session, identity)
    if session.check_only:
        if identity == AD_HOC_IDENTITY:
            print("Signing material is ready; the target will be signed ad hoc.", flush=True)
        else:
            print("Signing material is ready.", flush=True)
        return
    sign_artifact(
        session.target,
        identity=identity,
        environment=session.environment,
        entitlements=session.entitlements,
        keychain=keychain.path if keychain is not None and session.certificate is not None else None,
    )
    if session.credentials is not None:
        if keychain is None:
            raise PackError("The notarisation keychain is unavailable.", EXIT_SIGNING)
        notarize_and_staple(
            session.target,
            session.credentials,
            keychain=keychain,
            environment=session.environment,
        )


def main(arguments: list[str] | None = None) -> int:
    parsed = create_parser().parse_args(arguments)
    try:
        require_macos("macOS signing")
        session = create_session(parsed)
        if _keychain_required(session):
            with TemporaryKeychain() as keychain:
                perform(session, keychain)
        else:
            perform(session, None)
        return 0
    except PackError as exception:
        print(f"Error: {exception}", file=sys.stderr, flush=True)
        return exception.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
