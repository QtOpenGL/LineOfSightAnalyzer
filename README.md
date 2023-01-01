# Line Of Sight Analyzer
This line of sight analyzer is implemented without using ray casting techniques. Instead, I used omnidirectional shadow mapping idea in order to generate LOS map.
The application runs at 144 FPS (V-Sync enabled) on my PC.

## Build
1) Install `CMake 3.25.1`.
2) Install `Visual Studio 2019 and MSVC C++ Compiler`.
3) Install `Qt 5.15.2 MSVC2019 64bit` kit.
4) Clone the repo `git clone https://github.com/berkbavas/LineOfSightAnalyzer.git`.
5) Create a folder `mkdir Build`.
6) Enter the folder `cd Build`.
7) Run CMake `cmake ..`.
8) Open `LineOfSightAnalyzer.sln` with `Visual Studio 2019`.
9) Build & Run.

## Screenshots

![image](https://user-images.githubusercontent.com/53399385/201484129-67f08fa8-5f10-4456-af9e-e9180bfe404d.png)

![image](https://user-images.githubusercontent.com/53399385/201484169-9099b497-c876-4145-8332-6c7a50e5621d.png)
