#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"

PYTHON_BIN=${PYTHON:-python3}
if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
    echo "Python 3 was not found. Install it or set PYTHON to its executable." >&2
    exit 1
fi

exec "$PYTHON_BIN" - <<'PYTHON'
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def main():
    if sys.version_info.major != 3:
        raise RuntimeError("Python 3 is required.")
    if shutil.which("git") is None:
        raise RuntimeError("Git was not found on PATH.")

    root = Path.cwd().resolve()
    branch = os.environ.get("LUDORK_WORKSPACE_BRANCH") or "gh-pages"
    local_ref = f"refs/heads/{branch}"
    remote_ref = f"refs/remotes/origin/{branch}"
    remote_branch = f"origin/{branch}"
    fetch_spec = f"{local_ref}:{remote_ref}"

    def git(*arguments, directory=root):
        return subprocess.run(
            ["git", "-C", str(directory), *arguments],
            stdout=subprocess.PIPE,
            check=True,
        ).stdout

    def git_path(argument, directory=root):
        value = os.fsdecode(git("rev-parse", argument, directory=directory)).rstrip("\n")
        return (directory / value).resolve()

    def has_ref(reference):
        result = subprocess.run(
            ["git", "-C", str(root), "show-ref", "--verify", "--quiet", reference]
        )
        if result.returncode not in (0, 1):
            result.check_returncode()
        return result.returncode == 0

    if git_path("--show-toplevel") != root:
        raise RuntimeError("The script must belong to the root of a Git worktree.")
    if os.fsdecode(git("check-ref-format", "--branch", branch)).rstrip("\n") != branch:
        raise RuntimeError("LUDORK_WORKSPACE_BRANCH must be a literal branch name.")

    matching_worktrees = []
    for record in git("worktree", "list", "--porcelain", "-z").split(b"\0\0"):
        fields = record.split(b"\0")
        if os.fsencode(f"branch {local_ref}") in fields:
            matching_worktrees.append(Path(os.fsdecode(fields[0][len(b"worktree "):])))
    if len(matching_worktrees) > 1:
        raise RuntimeError(f"Multiple {branch} worktrees are registered; resolve the duplicate checkouts first.")
    target = matching_worktrees[0] if matching_worktrees else None

    if target is not None:
        if not target.is_dir():
            raise RuntimeError(f"The registered {branch} worktree is missing: {target}")
        if (
            git_path("--show-toplevel", target) != target.resolve()
            or git_path("--git-common-dir", target) != git_path("--git-common-dir")
            or git("symbolic-ref", "--quiet", "HEAD", directory=target).rstrip(b"\n")
            != os.fsencode(local_ref)
        ):
            raise RuntimeError(f"The registered {branch} worktree is invalid: {target}")
        if target.resolve() == root:
            raise RuntimeError(f"Run this script from a different worktree than {branch}.")
    else:
        directory_branch = re.sub(r'[<>:"/\\|?*]', "-", branch)
        target = root.parent / f"Ludork-{directory_branch}"
        if target.exists() and (not target.is_dir() or any(target.iterdir())):
            raise RuntimeError(f"The worktree directory is already occupied: {target}")

    try:
        target_path = os.path.relpath(target, root)
    except ValueError:
        target_path = str(target.resolve())
    if os.sep == "\\":
        target_path = target_path.replace("\\", "/")
    workspace = root / (root.name + ".code-workspace")
    if workspace.is_dir():
        raise RuntimeError(f"The workspace path is a directory: {workspace}")
    if workspace.exists() and not os.access(workspace, os.W_OK):
        raise RuntimeError(f"The workspace file is read-only: {workspace}")
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n", dir=root,
            prefix=".create-vsc-workspace-", suffix=".tmp", delete=False,
        ) as output:
            temporary_path = Path(output.name)
            json.dump({"folders": [{"path": "."}, {"path": target_path}]}, output, indent=2)
            output.write("\n")
        if not matching_worktrees:
            if has_ref(local_ref):
                git("worktree", "add", "--", str(target), branch)
            else:
                if not has_ref(remote_ref):
                    git(
                        "fetch",
                        "--no-tags",
                        "origin",
                        fetch_spec,
                    )
                git(
                    "-c", f"remote.origin.fetch=+{fetch_spec}",
                    "worktree", "add", "--track", "-b", branch, "--", str(target), remote_branch,
                )
                if not git("for-each-ref", "--format=%(upstream)", local_ref).strip():
                    git("config", "--add", "remote.origin.fetch", f"+{fetch_spec}")
        os.replace(temporary_path, workspace)
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()
    print(f"Created VS Code workspace: {workspace}")
    print(f"{branch} worktree: {target}")


try:
    main()
except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
    print(f"Could not create VS Code workspace: {error}", file=sys.stderr)
    sys.exit(1)
PYTHON
