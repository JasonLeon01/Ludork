#!/bin/bash

ENV_DIR=LudorkEnv
source "$ENV_DIR/bin/activate"
cd C_Extensions
python3 build.py
