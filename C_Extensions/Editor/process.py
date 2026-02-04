# -*- encoding: utf-8 -*-

import os
import sys
import shutil
import subprocess
from typing import List

ExtensionName = "EditorExtension"
ProjectFolder = "../../"
EngineFolder = ProjectFolder + "Sample/Engine/"
TargetFolder = ProjectFolder + "EditorExtensions/"


def collectFileNames() -> List[str]:
    result = []
    for file in os.listdir(os.getcwd()):
        if file.endswith(".pyd") or file.endswith(".so") or file.endswith(".dll"):
            result.append(file)
    return result


os.makedirs(TargetFolder, exist_ok=True)
pysfDir = EngineFolder + "pysf/"
targetNames = collectFileNames()
print(f"Generating {ExtensionName}.pyi in {os.path.abspath(os.getcwd())}")
subprocess.run(
    [sys.executable, "-m", "pybind11_stubgen", "--output-dir=.", ExtensionName],
    cwd=os.path.abspath(os.getcwd()),
)
print("Complete generation!")
for targetFile in targetNames:
    shutil.move(os.path.join(os.getcwd(), targetFile), os.path.join(TargetFolder, targetFile))
shutil.move(os.path.join(os.getcwd(), ExtensionName + ".pyi"), os.path.join(TargetFolder, ExtensionName + ".pyi"))
