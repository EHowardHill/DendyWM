#!/bin/bash

set -e

# Copy binaries
cd src

    # Dendy Window Manager
    echo "🏢: Window Manager"
    cd dendy_wm
        make clean
        make
        cp dendy_wm ../../dendy/etc/dendy/dendy_wm
        echo ""
    cd ..

cd ..

sudo chmod 0775 dendy/DEBIAN/*
dpkg-deb -b dendy ./builds