#!/bin/bash

# Script to install dependencies for the kiosk compositor

echo "Installing dependencies for kiosk compositor..."

# Detect distribution
if [ -f /etc/debian_version ]; then
    # Debian/Ubuntu
    echo "Detected Debian/Ubuntu system"
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        pkg-config \
        libwayland-dev \
        libwlroots-dev \
        libxkbcommon-dev \
        wayland-protocols \
        libpixman-1-dev \
        libdrm-dev \
        libgbm-dev \
        libegl1-mesa-dev \
        libgles2-mesa-dev \
        libinput-dev \
        libxcb1-dev \
        libxcb-composite0-dev \
        libxcb-xfixes0-dev \
        libxcb-xinput-dev \
        libxcb-render0-dev \
        libx11-xcb-dev \
        wayland-scanner

elif [ -f /etc/fedora-release ]; then
    # Fedora
    echo "Detected Fedora system"
    sudo dnf install -y \
        gcc-c++ \
        pkgconfig \
        wayland-devel \
        wlroots-devel \
        libxkbcommon-devel \
        wayland-protocols-devel \
        pixman-devel \
        libdrm-devel \
        mesa-libgbm-devel \
        mesa-libEGL-devel \
        mesa-libGLES-devel \
        libinput-devel \
        libxcb-devel \
        wayland-scanner

elif [ -f /etc/arch-release ]; then
    # Arch Linux
    echo "Detected Arch Linux system"
    sudo pacman -S --needed \
        base-devel \
        pkgconf \
        wayland \
        wlroots \
        libxkbcommon \
        wayland-protocols \
        pixman \
        libdrm \
        mesa \
        libinput \
        libxcb \
        wayland-scanner

else
    echo "Unsupported distribution. Please install the following packages manually:"
    echo "- wayland development headers"
    echo "- wlroots development headers"
    echo "- xkbcommon development headers"
    echo "- wayland-protocols"
    echo "- wayland-scanner"
    echo "- C++ compiler (g++ or clang++)"
    echo "- pkg-config"
    exit 1
fi

echo ""
echo "Dependencies installed. You can now run ./build.sh to compile the compositor."