import os
import sys
import shutil
import subprocess
import stat
import argparse
import tomllib

ROOT = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(ROOT, ".."))
BUILD_DIR = os.path.join(ROOT, "build")
PYSF_DIR = os.path.join(PROJECT_ROOT, "pysf")

sys.path.insert(0, ROOT)


def loadConfig():
    with open(os.path.join(ROOT, "extensions.toml"), "rb") as f:
        return tomllib.load(f)


def handleRemoveReadonly(func, path, excinfo):
    if isinstance(excinfo[1], PermissionError):
        os.chmod(path, stat.S_IWRITE)
        func(path)
    else:
        raise


def cmakeBuild(clean=True):
    if clean and os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR, onerror=handleRemoveReadonly)
    os.makedirs(BUILD_DIR, exist_ok=True)
    subprocess.run(
        ["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"],
        cwd=BUILD_DIR,
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", ".", "--config", "Release"],
        cwd=BUILD_DIR,
        check=True,
    )


def findArtifacts(name: str) -> list:
    artifacts = []
    for dirpath, _dirnames, filenames in os.walk(BUILD_DIR):
        for f in filenames:
            if f.startswith(name) and (f.endswith(".pyd") or f.endswith(".so") or f.endswith(".dylib")):
                artifacts.append(os.path.join(dirpath, f))
    return artifacts


def generateStub(name: str, work_dir: str, patches: list):
    subprocess.run(
        [sys.executable, "-m", "pybind11_stubgen", "--output-dir=.", name],
        cwd=work_dir,
        check=True,
    )
    stub_path = os.path.join(work_dir, f"{name}.pyi")
    if patches and os.path.exists(stub_path):
        with open(stub_path, "r", encoding="utf-8") as f:
            content = f.read()
        for patch in patches:
            content = content.replace(patch["find"], patch["replace"])
        with open(stub_path, "w", encoding="utf-8") as f:
            f.write(content)
    return stub_path


def distribute(config: dict, only: str = None):
    for _key, ext in config.items():
        name = ext["name"]
        if only and name != only:
            continue

        target_dir = os.path.join(PROJECT_ROOT, ext["target_dir"])
        os.makedirs(target_dir, exist_ok=True)
        needs_pysf = ext.get("needs_pysf", False)
        patches = ext.get("stub_patches", [])

        artifacts = findArtifacts(name)
        if not artifacts:
            print(f"[WARN] No artifacts found for {name}, skipping")
            continue

        if needs_pysf:
            for art in artifacts:
                shutil.copy2(art, PYSF_DIR)
            generateStub(name, PYSF_DIR, patches)
            for art in artifacts:
                fname = os.path.basename(art)
                shutil.move(os.path.join(PYSF_DIR, fname), os.path.join(target_dir, fname))
            stub = os.path.join(PYSF_DIR, f"{name}.pyi")
            if os.path.exists(stub):
                shutil.move(stub, os.path.join(target_dir, f"{name}.pyi"))
        else:
            for art in artifacts:
                shutil.copy2(art, target_dir)
            generateStub(name, target_dir, patches)

        print(f"[OK] {name} -> {target_dir}")


def runBindgen(only: str = None):
    """运行自动绑定生成器"""
    try:
        from bindgen.generate import main as bindgen_main
        print("[BUILD] Running bindgen...")
        sys.argv = ["bindgen"]
        if only:
            sys.argv.extend(["--only", only])
        bindgen_main()
    except ImportError as e:
        print(f"[WARN] Bindgen not available ({e}), skipping code generation")
        print("[HINT] Install libclang: pip install libclang")
    except Exception as e:
        print(f"[WARN] Bindgen failed: {e}")
        print("[HINT] Falling back to existing binding files")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-clean", dest="clean", action="store_false")
    parser.add_argument("--only", type=str, default=None)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-bindgen", action="store_true", help="Skip auto binding generation")
    args = parser.parse_args()

    config = loadConfig()
    if not args.skip_bindgen:
        runBindgen(only=args.only)
    if not args.skip_build:
        cmakeBuild(clean=args.clean)
    distribute(config, only=args.only)


if __name__ == "__main__":
    main()
