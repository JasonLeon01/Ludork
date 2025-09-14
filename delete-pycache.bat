@echo off
set "target_dir=%~dp0"

echo Searching and deleting __pycache__ folders in %target_dir%

for /r "%target_dir%" %%d in (.) do (
    if exist "%%d\__pycache__" (
        echo Deleting "%%d\__pycache__"
        rd /s /q "%%d\__pycache__"
    )
)

echo All __pycache__ folders deleted.
pause
