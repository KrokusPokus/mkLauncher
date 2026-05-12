#!/bin/bash

if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

if [ -f "./bin/mkLauncher" ]; then
    strip ./bin/mkLauncher

    if [ ! -d "$HOME/.local/bin" ]; then
        mkdir "$HOME/.local/bin"
    fi

    cp ./bin/mkLauncher $HOME/.local/bin/
fi
