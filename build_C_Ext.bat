@echo off

set ENV_DIR=LudorkEnv
call "%ENV_DIR%\Scripts\activate.bat"
cd C_Extensions
python build.py
