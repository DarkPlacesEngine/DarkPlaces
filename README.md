# DarkPlaces Engine

DarkPlaces is a game engine based on the Quake 1 engine by id Software. It
improves and builds upon the original 1996 engine by adding modern rendering
features, and expanding upon the engine's native game code language QuakeC, as
well as supporting additional map and model formats.

Developed by LadyHavoc. See CREDITS.md for a list of contributors.

## Help/support

### IRC:
#darkplaces on irc.anynet.org

### Discord:
https://discord.gg/ZHT9QeW

## Build instructions (WIP)

You will need the following packages regardless of platform:
* SDL2
* libjpeg
* libpng
* libvorbis
* libogg

### Windows (MSYS2):

1. Install MSYS2, found [here](https://www.msys2.org/).
2. Once you've installed MSYS2 and have fully updated it, open a MinGW64 terminal (***not an MSYS2 terminal***) and input the following command:

```
pacman -S --needed gcc make mingw-w64-x86_64-{toolchain,libjpeg-turbo,libpng,libogg,libvorbis,SDL2}
```

3. See [Unix instructions](#unix-(general)).

### Unix (General)

In the engine's root directory, run `make`. See `make help` for options.

### Windows (Visual Studio)

Instructions coming soon.

## Documentation

Doxygen: https://xonotic.org/doxygen/darkplaces

