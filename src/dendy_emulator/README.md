Simple C++ Libretro Launcher
This is a minimal C++ application for Debian 12 (x86_64) that launches a libretro core in fullscreen based on the file extension of a provided ROM file.

1. Dependencies
You must install the necessary development libraries and the libretro cores you wish to use.

Open a terminal and run the following commands:

# Install the C++ compiler and SDL2 development library
sudo apt update
sudo apt install build-essential libsdl2-dev

# Install the libretro cores you want to use.
# Here are some common examples:
sudo apt install mgba-libretro         # For Game Boy Advance (.gba)
sudo apt install snes9x-libretro       # For Super Nintendo (.sfc, .smc)
sudo apt install fceumm-libretro       # For Nintendo Entertainment System (.nes)
sudo apt install genesis-plus-gx-libretro # For Sega Genesis/Mega Drive (.md, .gen)

2. Compilation
Place the main.cpp and Makefile in the same directory. Navigate to that directory in your terminal and run the make command:

make

This will create an executable file named retro-launcher in the same directory.

3. Installation (Optional)
If you want to make the retro-launcher command available system-wide, you can install it:

sudo make install

This will copy the executable to /usr/local/bin, which is in your system's PATH.

4. Usage
Run the application from the terminal, providing the full path to your ROM file as the only argument.

If you didn't install it:

./retro-launcher /path/to/your/game.gba

If you installed it:

retro-launcher /path/to/your/game.sfc

The application will open in fullscreen. To exit, press the Escape key.

5. Controls
The application supports both keyboard and the first detected joystick. If a joystick is present, it will be used for input.

Keyboard Controls
D-Pad: Arrow Keys

A Button: Z

B Button: X

X Button: A

Y Button: S

Start: Enter

Select: Right Shift

L Shoulder: Q

R Shoulder: W

Exit Emulator: Escape

GameCube Controller (Assumed Mapping)
This is a typical mapping for a USB GameCube adapter. Your controller may vary.

D-Pad: D-Pad on controller

A Button: 'A' face button

B Button: 'B' face button

X Button: 'X' face button

Y Button: 'Y' face button

Start: 'Start' button

Select: 'Z' trigger (often used as a substitute)

L Shoulder: 'L' shoulder button

R Shoulder: 'R' shoulder button