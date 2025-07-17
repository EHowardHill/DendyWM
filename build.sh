#!/bin/bash

set -e

# Copy binaries
cd src

    echo "🏢: Window Manager"
    cd dendy_wm
        g++ dendy_wm.cpp -o dendy_wm -lX11
        mv dendy_wm ../../dendy/etc/dendy/dendy_wm
        echo ""
    cd ..

    echo "🏢: Launcher"
    cd dendy_launcher
        make -j$(nproc)
        mv dendy_launcher ../../dendy/etc/dendy/dendy_launcher
        echo ""
    cd ..

cd ..

sudo chmod 0775 dendy/DEBIAN/*
dpkg-deb -b dendy ./builds