# -*- encoding: utf-8 -*-

import os
import sys
import shutil
import subprocess
from typing import List

ExtensionName = "UtilsExtension"
EngineFolder = "../../Sample/Engine/"
TargetFolder = EngineFolder


def collectFileNames() -> List[str]:
    result = []
    for file in os.listdir(os.getcwd()):
        if file.endswith(".pyd") or file.endswith(".so") or file.endswith(".dll"):
            result.append(file)
    return result


os.makedirs(TargetFolder, exist_ok=True)
pysfDir = EngineFolder + "pysf/"
targetNames = collectFileNames()
for targetFile in targetNames:
    shutil.move(os.path.join(os.getcwd(), targetFile), os.path.join(pysfDir, targetFile))
print(f"Generating {ExtensionName}.pyi in {os.path.abspath(os.path.join(os.getcwd(), pysfDir))}")
subprocess.run(
    [sys.executable, "-m", "pybind11_stubgen", "--output-dir=.", ExtensionName],
    cwd=os.path.abspath(os.path.join(os.getcwd(), pysfDir)),
)
print("Complete generation!")
for targetFile in targetNames:
    shutil.move(os.path.join(pysfDir, targetFile), os.path.join(TargetFolder, targetFile))
with open(os.path.join(pysfDir, ExtensionName + ".pyi"), "r") as f:
    content = f.read()
    content = content.replace("import pysf.sf", "from Engine import pysf")
    content = content.replace("pysf.sf.", "pysf.")
    with open(os.path.join(pysfDir, ExtensionName + ".pyi"), "w") as f:
        f.write(content)
shutil.move(os.path.join(pysfDir, ExtensionName + ".pyi"), os.path.join(TargetFolder, ExtensionName + ".pyi"))
