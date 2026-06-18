#!/usr/bin/env bash

bash cleanup.sh

cd "$(dirname "$0")"

source versions.conf

mkdir -p "C_Extensions"
if [ -d "C_Extensions/SFML" ]; then
  rm -rf "C_Extensions/SFML"
fi

echo "Downloading SFML..."
curl -L -o sfml.tar.gz "https://github.com/SFML/SFML/archive/refs/tags/${SFML_VERSION}.tar.gz"
if [ $? -ne 0 ]; then
  echo "Failed to download SFML."
  exit 1
fi

echo "Extracting SFML..."
tar -xzf sfml.tar.gz -C "C_Extensions"
if [ $? -ne 0 ]; then
  echo "Failed to extract SFML."
  rm sfml.tar.gz
  exit 1
fi

rm sfml.tar.gz

if [ -d "C_Extensions/SFML-${SFML_VERSION}" ]; then
  mv "C_Extensions/SFML-${SFML_VERSION}" "C_Extensions/SFML"
else
  echo "SFML source folder not found."
  exit 1
fi

if [ -d "Sample/pysf" ]; then
  rm -rf "Sample/pysf"
fi

if [ -d "pysf" ]; then
  rm -rf "pysf"
fi

echo "Downloading PySF..."
curl -L -o pysf.zip "https://github.com/JasonLeon01/PySF-AutoGenerator/releases/download/PySF${PYSF_VERSION}/pysf-macOS-ARM64.zip"
if [ $? -ne 0 ]; then
  echo "Failed to download PySF."
  exit 1
fi

echo "Extracting PySF..."
unzip -q pysf.zip -d "."
if [ $? -ne 0 ]; then
  echo "Failed to extract PySF to current folder."
  rm pysf.zip
  exit 1
fi
rm pysf.zip

echo "Downloading PySF for iOS..."
curl -L -o pysf_ios.zip "https://github.com/JasonLeon01/PySF-AutoGenerator/releases/download/PySF${PYSF_VERSION}/pysf-iOS-ARM64.zip"
if [ $? -ne 0 ]; then
  echo "Failed to download PySF for iOS."
  exit 1
fi

echo "Extracting PySF for iOS..."
mkdir -p "pysf_ios_tmp"
unzip -q pysf_ios.zip -d "pysf_ios_tmp"
if [ $? -ne 0 ]; then
  echo "Failed to extract PySF for iOS."
  rm -rf pysf_ios_tmp pysf_ios.zip
  exit 1
fi

echo "Merging iOS PySF into pysf..."
cp -R pysf_ios_tmp/*/pysf/* pysf/ 2>/dev/null || cp -R pysf_ios_tmp/pysf/* pysf/ 2>/dev/null || cp pysf_ios_tmp/* pysf/ 2>/dev/null
rm -rf pysf_ios_tmp pysf_ios.zip

if [ -d "ios_python" ]; then
  rm -rf "ios_python"
fi
mkdir -p "ios_python"

echo "Downloading Python-Apple-support (iOS)..."
IOS_PY_VER="${IOS_PYTHON_TAG%-*}"
IOS_SUP_VER="${IOS_PYTHON_TAG#*-}"
curl -L -o ios_python.tar.gz "https://github.com/beeware/Python-Apple-support/releases/download/${IOS_PYTHON_TAG}/Python-${IOS_PY_VER}-iOS-support.${IOS_SUP_VER}.tar.gz"
if [ $? -ne 0 ]; then
  echo "Failed to download Python-Apple-support."
  exit 1
fi

echo "Extracting Python-Apple-support..."
tar -xzf ios_python.tar.gz -C "ios_python" --strip-components=1
if [ $? -ne 0 ]; then
  echo "Failed to extract Python-Apple-support."
  rm ios_python.tar.gz
  exit 1
fi
rm ios_python.tar.gz

ENV_DIR="LudorkEnv"
PY_CMD="python3.12"

if ! command -v "$PY_CMD" >/dev/null 2>&1; then
  echo "Python 3.12 not found. Please install Python 3.12 and try again."
  exit 1
fi

if [ -f "$ENV_DIR/bin/activate" ]; then
  . "$ENV_DIR/bin/activate"
  PV=$(python --version 2>&1 | awk '{print $2}')
  case "$PV" in
    3.12*)
      ;;
    *)
      deactivate 2>/dev/null || true
      rm -rf "$ENV_DIR"
      ;;
  esac
fi

if [ ! -f "$ENV_DIR/bin/activate" ]; then
  "$PY_CMD" -m venv "$ENV_DIR"
  if [ $? -ne 0 ]; then
    rm -rf "$ENV_DIR"
    exit 1
  fi

  . "$ENV_DIR/bin/activate"
  "$PY_CMD" -m pip install -r requirements.txt
  if [ $? -ne 0 ]; then
    rm -rf "$ENV_DIR"
    exit 1
  fi
fi

bash tools/apply_nodegraphqt_patch.sh
bash ./build_C_Ext.sh

"$PY_CMD" main.py
exit $?
