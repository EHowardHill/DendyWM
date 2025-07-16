#!/bin/bash

set -e

# Copy binaries
cd src

    # Dendy Window Manager
    echo "🏢: Window Manager"
    cd dendy_wm
        g++ dendy_wm.cpp -o dendy_wm -lX11
        chmod +x dendy_wm
        mv dendy_wm ../../dendy/etc/dendy/dendy_wm
        echo ""
    cd ..

cd ..

sudo chmod 0775 dendy/DEBIAN/*
dpkg-deb -b dendy ./builds