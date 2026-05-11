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
BUILD_IOS_DIR = os.path.join(ROOT, "build_ios")
PYSF_DIR = os.path.join(PROJECT_ROOT, "pysf")
IOS_PYTHON_DIR = os.path.join(PROJECT_ROOT, "ios_python")
IOS_PYTHON_VERSION = "3.12"
IOS_DEPLOY_TARGET = "15.0"
IOS_ARCH = "arm64"

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


def detectIOSPython():
    candidates = [
        (
            os.path.join(IOS_PYTHON_DIR, "include", f"python{IOS_PYTHON_VERSION}"),
            os.path.join(IOS_PYTHON_DIR, "lib", f"libpython{IOS_PYTHON_VERSION}.a"),
        ),
        (
            os.path.join(IOS_PYTHON_DIR, "Python.xcframework", "ios-arm64", "Headers"),
            os.path.join(IOS_PYTHON_DIR, "Python.xcframework", "ios-arm64", "Python"),
        ),
        (
            os.path.join(IOS_PYTHON_DIR, "Python.xcframework", "ios-arm64", "Headers"),
            os.path.join(IOS_PYTHON_DIR, "Python.xcframework", "ios-arm64", f"libPython{IOS_PYTHON_VERSION}.a"),
        ),
        (
            os.path.join(IOS_PYTHON_DIR, "Python.xcframework", "ios-arm64", "Python.framework", "Headers"),
            os.path.join(IOS_PYTHON_DIR, "Python.xcframework", "ios-arm64", "Python.framework", "Python"),
        ),
    ]
    for inc, lib in candidates:
        if os.path.isdir(inc) and os.path.exists(lib):
            return inc, lib
    raise RuntimeError(
        "Could not locate iOS Python development files under: "
        f"{IOS_PYTHON_DIR}. Run init.sh to download Python-Apple-support."
    )


def detectHostPython():
    if sys.executable:
        return sys.executable
    for cmd in ("python3.12", "python3"):
        path = shutil.which(cmd)
        if path:
            return path
    raise RuntimeError("No host Python interpreter found.")


def cmakeBuildIOS(clean=True):
    if clean and os.path.exists(BUILD_IOS_DIR):
        shutil.rmtree(BUILD_IOS_DIR, onerror=handleRemoveReadonly)
    os.makedirs(BUILD_IOS_DIR, exist_ok=True)

    inc, lib = detectIOSPython()
    host_py = detectHostPython()
    print(f"[iOS] host python: {host_py}")
    print(f"[iOS] python include: {inc}")
    print(f"[iOS] python library: {lib}")

    subprocess.run(
        [
            "cmake", "..",
            "-G", "Xcode",
            "-DLUDORK_IOS=ON",
            "-DCMAKE_SYSTEM_NAME=iOS",
            f"-DCMAKE_OSX_DEPLOYMENT_TARGET={IOS_DEPLOY_TARGET}",
            f"-DCMAKE_OSX_ARCHITECTURES={IOS_ARCH}",
            f"-DPython3_EXECUTABLE={host_py}",
            f"-DPYTHON_EXECUTABLE={host_py}",
            f"-DPython_EXECUTABLE={host_py}",
            f"-DPython3_INCLUDE_DIR={inc}",
            f"-DPython3_LIBRARY={lib}",
            f"-DPYTHON_INCLUDE_DIR={inc}",
            f"-DPYTHON_LIBRARY={lib}",
        ],
        cwd=BUILD_IOS_DIR,
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", ".", "--config", "Release"],
        cwd=BUILD_IOS_DIR,
        check=True,
    )


def findIOSStaticLibs(name: str) -> list:
    artifacts = []
    for dirpath, _dirnames, filenames in os.walk(BUILD_IOS_DIR):
        for f in filenames:
            if f == f"lib{name}.a" or f == f"{name}.a":
                artifacts.append(os.path.join(dirpath, f))
    return artifacts


def collectIOSDependencyLibs() -> list:
    libs = []
    for dirpath, _dirnames, filenames in os.walk(BUILD_IOS_DIR):
        for f in filenames:
            if f.endswith(".a") and f.startswith("libsfml-"):
                libs.append(os.path.join(dirpath, f))
    return libs


def distributeIOS(config: dict, only: str = None):
    sfml_libs = collectIOSDependencyLibs()
    for _key, ext in config.items():
        name = ext["name"]
        if only and name != only:
            continue
        if not ext.get("ios_enabled", False):
            continue

        target_dir = os.path.join(PROJECT_ROOT, ext["target_dir"])
        os.makedirs(target_dir, exist_ok=True)

        artifacts = findIOSStaticLibs(name)
        if not artifacts:
            print(f"[WARN] No iOS artifacts found for {name}, skipping")
            continue

        for art in artifacts:
            shutil.copy2(art, target_dir)

        if ext.get("needs_pysf", False):
            for sfml_lib in sfml_libs:
                shutil.copy2(sfml_lib, target_dir)

        print(f"[OK] iOS {name} -> {target_dir}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-clean", dest="clean", action="store_false")
    parser.add_argument("--only", type=str, default=None)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-bindgen", action="store_true", help="Skip auto binding generation")
    parser.add_argument("--ios", action="store_true", help="Cross-compile static libraries for iOS arm64")
    args = parser.parse_args()

    config = loadConfig()

    if args.ios:
        if sys.platform != "darwin":
            print("[ERROR] --ios is only supported on macOS host.")
            sys.exit(1)
        if not args.skip_build:
            cmakeBuildIOS(clean=args.clean)
        distributeIOS(config, only=args.only)
        return

    if not args.skip_bindgen:
        runBindgen(only=args.only)
    if not args.skip_build:
        cmakeBuild(clean=args.clean)
    distribute(config, only=args.only)


if __name__ == "__main__":
    main()
