## BetterTelekinesis 

SKSE port of BetterTelekinesis by meh321 using Commonnlib. Currently supports SE and AE. VR support planned. Config options have remained the same, the new config is BetterTelekinesis.ini in SKSE/Plugins. All features except Dragon Telekinesis should be functional in SE and AE. Currently planned are further cleanup and refactoring, a switch to Commonlib-NG and VR support, and bug fixes.

## Current Issues

* With Multi-Telekinesis some objects are aren't pulled all the way causing them to not be launched
* The Target Picker will sometimes not grab the closest object 
* Telekinesis label is not function properly


## Requirements to Build

- [Casual Library](https://github.com/CasualCoder91/CasualLibrary/)
  - Compile or add libs from the github releases and add to `/external/CasualLibrary1.0/`
- [CMake](https://cmake.org/)
  - Add this to your `PATH`
- [The Elder Scrolls V: Skyrim Special Edition](https://store.steampowered.com/app/489830)
  - Add the environment variable `Skyrim64Path` to point to the root installation of your game directory (the one containing `SkyrimSE.exe`).
- [Vcpkg](https://github.com/microsoft/vcpkg)
  - Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
  - Desktop development with C++

* [CommonLibSSE](https://github.com/powerof3/CommonLibSSE/tree/dev)
  - You need to build from the powerof3/dev branch
  - Add this as as an environment variable `CommonLibSSEPath`
* [CommonLibVR](https://github.com/alandtse/CommonLibVR/tree/vr)
  - You need to build from the alandtse/vr branch
  - Add this as as an environment variable `CommonLibVRPath` instead of /external

## User Requirements

- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
  - Needed for SSE/AE
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
  - Needed for VR

## Register Visual Studio as a Generator

- Open `x64 Native Tools Command Prompt`
- Run `cmake`
- Close the cmd window

## Building

```
git clone https://github.com/SaneEngineer/BetterTelekinesis.git
cd BetterTelekinesis
# pull commonlib /extern to override the path settings
git submodule update --init --recursive
```

### SSE

```
cmake --preset vs2022-windows-vcpkg
cmake --build build --config Release
```

### AE

```
cmake --preset vs2022-windows-vcpkg
cmake --build buildae --config Release
```

### VR

```
cmake --preset vs2022-windows-vcpkg-vr
cmake --build buildvr --config Release
```

## License
