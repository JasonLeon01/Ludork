from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


WORKSPACE_BRANCHES = [
    "gh-pages",
    "Server",
]


def main() -> None:
    if sys.version_info.major != 3:
        raise RuntimeError("Python 3 is required.")
    if shutil.which("git") is None:
        raise RuntimeError("Git was not found on PATH.")

    root = Path(__file__).resolve().parent.parent
    branches = unique_branches(WORKSPACE_BRANCHES)
    if not same_path(git_path(root, "--show-toplevel"), root):
        raise RuntimeError("The tools directory must belong to the Git project root.")
    for branch_name in branches:
        validate_branch_name(root, branch_name)

    worktrees_by_branch = read_worktrees(root)
    plans = [resolve_branch(root, worktrees_by_branch, branch_name) for branch_name in branches]
    if all(plan["current"] for plan in plans):
        raise RuntimeError(
            f"{plans[0]['branch']} is checked out in the project directory; another folder is required."
        )
    seen_directories: dict[str, str] = {}
    for plan in plans:
        if plan["current"]:
            continue
        directory_key = os.path.normcase(str(plan["target"]))
        previous = seen_directories.get(directory_key)
        if previous is not None:
            raise RuntimeError(
                f"Branches {previous} and {plan['branch']} use the same worktree directory: {plan['target']}"
            )
        seen_directories[directory_key] = plan["branch"]

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
            folders = [{"path": "."}]
            folders.extend(
                {"path": workspace_folder(root, plan["target"])}
                for plan in plans
                if not plan["current"]
            )
            json.dump({"folders": folders}, output, indent=2, ensure_ascii=False)
            output.write("\n")
        for plan in plans:
            if plan["create"]:
                add_worktree(root, plan)
        os.replace(temporary_path, workspace)
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()
    print(f"Created workspace: {workspace}")
    for plan in plans:
        print(f"{plan['branch']} worktree: {plan['target']}")


def unique_branches(branches: list[str]) -> list[str]:
    unique: list[str] = []
    for branch in branches:
        if branch not in unique:
            unique.append(branch)
    if not unique:
        raise RuntimeError("WORKSPACE_BRANCHES must list at least one branch.")
    return unique


def git(root: Path, *arguments: str, directory: Path | None = None) -> bytes:
    return subprocess.run(
        ["git", "-C", str(directory or root), *arguments],
        stdout=subprocess.PIPE,
        check=True,
    ).stdout


def git_path(root: Path, argument: str, directory: Path | None = None) -> Path:
    base = directory or root
    value = os.fsdecode(git(root, "rev-parse", argument, directory=base)).rstrip("\n")
    return (base / value).resolve()


def has_ref(root: Path, reference: str) -> bool:
    result = subprocess.run(
        ["git", "-C", str(root), "show-ref", "--verify", "--quiet", reference]
    )
    if result.returncode not in (0, 1):
        result.check_returncode()
    return result.returncode == 0


def validate_branch_name(root: Path, branch_name: str) -> None:
    actual = os.fsdecode(git(root, "check-ref-format", "--branch", branch_name)).rstrip("\n")
    if actual != branch_name:
        raise RuntimeError(f"Invalid workspace branch: {branch_name}")


def read_worktrees(root: Path) -> dict[bytes, list[Path]]:
    worktrees: dict[bytes, list[Path]] = {}
    for record in git(root, "worktree", "list", "--porcelain", "-z").split(b"\0\0"):
        if not record:
            continue
        fields = record.split(b"\0")
        if not fields or not fields[0].startswith(b"worktree "):
            continue
        branch_field = next((field for field in fields if field.startswith(b"branch ")), None)
        if branch_field is None:
            continue
        worktrees.setdefault(branch_field[len(b"branch "):], []).append(
            Path(os.fsdecode(fields[0][len(b"worktree "):]))
        )
    return worktrees


def resolve_branch(root: Path, worktrees_by_branch: dict[bytes, list[Path]], branch_name: str) -> dict[str, object]:
    local_ref = f"refs/heads/{branch_name}"
    remote_ref = f"refs/remotes/origin/{branch_name}"
    remote_branch = f"origin/{branch_name}"
    fetch_spec = f"{local_ref}:{remote_ref}"
    matching = worktrees_by_branch.get(os.fsencode(local_ref), [])
    if len(matching) > 1:
        raise RuntimeError(
            f"Multiple {branch_name} worktrees are registered; resolve the duplicate checkouts first."
        )
    if matching:
        target = matching[0]
        if not target.is_dir():
            raise RuntimeError(f"The registered {branch_name} worktree is missing: {target}")
        if (
            not same_path(git_path(root, "--show-toplevel", target), target)
            or not same_path(git_path(root, "--git-common-dir", target), git_path(root, "--git-common-dir"))
            or git(root, "symbolic-ref", "--quiet", "HEAD", directory=target).rstrip(b"\n")
            != os.fsencode(local_ref)
        ):
            raise RuntimeError(f"The registered {branch_name} worktree is invalid: {target}")
        current = same_path(target, root)
    else:
        target = root.parent / f"Ludork-{worktree_directory_name(branch_name)}"
        current = False
        if target.exists() and (not target.is_dir() or any(target.iterdir())):
            raise RuntimeError(f"The worktree directory is already occupied: {target}")
    return {
        "branch": branch_name,
        "target": target,
        "local_ref": local_ref,
        "remote_ref": remote_ref,
        "remote_branch": remote_branch,
        "fetch_spec": fetch_spec,
        "create": not matching,
        "current": current,
    }


def add_worktree(root: Path, plan: dict[str, object]) -> None:
    local_ref = str(plan["local_ref"])
    target = str(plan["target"])
    branch_name = str(plan["branch"])
    if has_ref(root, local_ref):
        git(root, "worktree", "add", "--", target, branch_name)
        return
    remote_ref = str(plan["remote_ref"])
    fetch_spec = str(plan["fetch_spec"])
    if not has_ref(root, remote_ref):
        git(root, "fetch", "--no-tags", "origin", fetch_spec)
    git(
        root,
        "-c", f"remote.origin.fetch=+{fetch_spec}",
        "worktree", "add", "--track", "-b", branch_name, "--",
        target, str(plan["remote_branch"]),
    )
    if not git(root, "for-each-ref", "--format=%(upstream)", local_ref).strip():
        git(root, "config", "--add", "remote.origin.fetch", f"+{fetch_spec}")


def worktree_directory_name(branch_name: str) -> str:
    return re.sub(r'[<>:"/\\|?*]', "-", branch_name)


def workspace_folder(root: Path, target: Path) -> str:
    try:
        folder = os.path.relpath(target, root)
    except ValueError:
        folder = str(target.resolve())
    if os.sep == "\\":
        folder = folder.replace("\\", "/")
    return folder


def same_path(left: Path, right: Path) -> bool:
    return os.path.normcase(str(left.resolve())) == os.path.normcase(str(right.resolve()))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Could not create VS Code workspace: {error}", file=sys.stderr)
        sys.exit(1)
