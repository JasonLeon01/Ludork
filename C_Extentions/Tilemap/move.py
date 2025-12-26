import os
import shutil

target_folder = "../../Sample/Engine/GamePlay/"
os.makedirs(target_folder, exist_ok=True)

for root, dirs, files in os.walk("build"):
    for file in files:
        if file.endswith(".pyd") or file.endswith(".so"):
            file_path = os.path.join(root, file)
            shutil.move(file_path, os.path.join(target_folder, file))
