#!/bin/bash

set -e

# Copy binaries
cd src

    echo "🏢: Launcher"
    cd dendy_launcher
        make -j$(nproc)
        mv dendy_launcher ../../dendy/etc/dendy/launcher
        echo ""
    cd ..

cd ..

# Increment the build number
control_file="./dendy/DEBIAN/control"
version=$(grep '^Version:' "$control_file" | awk '{print $2}')
if [ -z "$version" ]; then
    echo "Version not found in control file"
    exit 1
fi
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Invalid version format: $version"
    exit 1
fi
IFS='.' read -r major minor revision buildnumber <<< "$version"
new_buildnumber=$((buildnumber + 1))
new_version="$major.$minor.$revision.$new_buildnumber"
sed -i "s/^Version: .*/Version: $new_version/" "$control_file"

# Package the build
sudo chmod 0775 dendy/DEBIAN/*
dpkg-deb -b dendy ./builds