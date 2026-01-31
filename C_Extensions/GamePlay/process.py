import os
import shutil
import subprocess

target_folder = "../../Sample/Engine/GamePlay/"
os.makedirs(target_folder, exist_ok=True)

subprocess.run(["python", "-m", "pybind11_stubgen", "--output-dir=.", "GamePlayExtension"])

for file in os.listdir(os.getcwd()):
    if file.endswith(".pyd") or file.endswith(".so") or file.endswith(".dll") or file.endswith(".pyi"):
        file_path = os.path.join(os.getcwd(), file)
        shutil.move(file_path, os.path.join(target_folder, file))
