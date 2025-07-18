sudo apt install build-essential libx11-dev retroarch-dev libsdl2-dev libretro-gtk-1-dev

git clone https://github.com/raysan5/raylib.git raylib
cd ./raylib/src/
    make -j$(nproc) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=SHARED
    sudo make install RAYLIB_LIBTYPE=SHARED
cd ../..