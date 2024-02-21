#!/bin/bash
git clone https://github.com/emscripten-core/emsdk .emsdk
cd .emsdk
./emsdk install latest
./emsdk activate latest
. emsdk_env.sh