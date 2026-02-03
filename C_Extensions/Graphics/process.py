# -*- encoding: utf-8 -*-

import os
import shutil
import subprocess
from typing import List

ExtensionName = "GraphicsExtension"
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
subprocess.run(["python", "-m", "pybind11_stubgen", "--output-dir=.", ExtensionName], cwd=pysfDir)
for targetFile in targetNames:
    shutil.move(os.path.join(pysfDir, targetFile), os.path.join(TargetFolder, targetFile))
shutil.move(os.path.join(pysfDir, ExtensionName + ".pyi"), os.path.join(TargetFolder, ExtensionName + ".pyi"))
