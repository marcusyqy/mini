# Mini
The mini engine for creating games. This may be renamed into the game later on. Or this may become something like Raylib.

## Requirements. 
Requires Vulkan-SDK to be installed. We will probably fix the Vulkan-SDK version in the future but for now anything above - 1.3.268.0 will do. 
We may have to ship the vulkan-1.lib and .so files with the repo project if we are unable to find a way to lock the sdk to a certain version.

Also requires python to be installed and a C++ compiler (`cl` for windows and any for linux). Will look to also support clang compiling in the future when our development environment is better.

## Development environment
(Optional) Run `python3 setup.py` to generate `compile_flags.txt` for some `clangd` intellisense. 

## Build
We provide `build.bat` and `build.sh` as our build systems. 
We probably need to also document what command line arguments they have. Also, we may switch to python or plain c++ files in the event that it is proving to be a hastle to maintain both shell and bat files.
If we don't have more dependencies it should be trivial.

### IMPT
If you don't like either, the scripts should be simple enough to read and compile with your own flags.
