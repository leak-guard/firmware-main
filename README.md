# Main board firmware

This repository contains all necessary files needed to build the firmware
for the main board in LeakGuard project. All STM32 BSP/CSP dependencies 
are included as submodules, to reduce the amount of mess in the project.

## Prerequisites

To build the project, you'll need:
+ CMake 3.10+
+ `arm-none-eabi-gcc` toolchain, version 12+ (must support C++20 features)

Additionally, to upload the firmware to the real board, you'll also need:
+ An STLink V2/V3 programmer
+ OpenOCD

## Build instructions

CMake is used as the build system, so the procedure looks as usual. 
Additionally, some CMake presets are provided for convenience.

First, configure the project by running the following command:
```
cmake --preset <debug|release>
```

A build folder will be created, namely `build/debug` or `build/release`, 
depending on the preset you choose. Now you can build the project using
```
cmake --build --preset <debug|release>-build
```

If you also want to flash the firmware onto the hardware, run
```
cmake --build --preset <debug|release>-flash
```
It will automatically build the project, if there are some uncompiled changes.


