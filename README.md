# BioMenace decompilation (WIP)

This is a work in progress decompilation/source code reconstruction of the game BioMenace,
released by Apogee Software in 1993 for MS-DOS.

Based on [K1n9_Duk3's reconstruction of Commander Keen 4 source code](https://github.com/sparky4/keen4-6).
BioMenace is based on the same engine and thus shares a lot of code with the 2nd Keen trilogy.
It's far from identical though - almost all of the game logic is completely unique to this game,
and many other parts of the code have also been modified.

To my knowledge, this is the first publicly released code reconstruction of the game's first episode. In the meantime, K1n9_Duk3 has also released a code reconstruction which covers all 3 episodes and some extra versions:
* [Announcement post](https://pckf.com/viewtopic.php?t=18039)
* [Download](https://k1n9duk3.shikadi.net/files/modding/bmsource.zip)


## Current state

This code produces a 100% identical binary to `BMENACE1.EXE` from the freeware release of the game (SHA-256 `b91ed9c1e8a7a47cff209401f50aa7bc2eca9b42738d7f6aa5e6b55ed35fae7a`). The shareware version v1.1 (SHA-256 `c47d1114263b8cf3f27b776c8a858b4f89dc59d1a2cccfdddffc194277adc008`) can also be perfectly reproduced. Episodes 2 and 3 are not currently covered.


## Compiling the code

### Original DOS build (byte-exact reproduction)

A copy of Borland C++ 2.0 is required, and a DOS environment to run it in (real or emulated).
The compiler is expected to be installed at `C:\BCC_20` by default.
The `BIN` subdirectory of the installation should be in the `PATH`.

Within the DOS environment, `cd` into the directory containing the code and run `make`.
This creates a file called `BMENACE1.EXE`, but it still needs to be compressed before it matches the original version.
Run `LZEXE\LZEXE.EXE BMENACE1.EXE` to do so, and a perfectly matching file should be produced.

To build the shareware version, uncomment the corresponding `#define` near the top of `ID_HEADS.H`, and comment out the `FREEWARE` `#define`.

### SDL port (Ubuntu 24.04, x86_64)

The SDL port builds the game as a native Linux binary using CMake and SDL2.
This is a work-in-progress port and does not yet produce a fully playable game.

**Prerequisites:**

Install the required packages:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libsdl2-dev
```

**Building:**

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

This produces a `bmenace1` executable in the `build/` directory.

**Debug build:**

To build with debug symbols and no optimization (useful for debugging with gdb):

```bash
mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

The `-DCMAKE_BUILD_TYPE=Debug` flag tells CMake to compile with `-g` (debug symbols) and `-O0` (no optimization), which makes debugging much easier since variables won't be optimized away and code will execute in source order.

Other useful build types:
- `RelWithDebInfo` — optimized build that still includes debug symbols (good for profiling or debugging release-mode-only issues)
- `Release` — optimized build with no debug symbols (smallest/fastest binary)

**Debugging with gdb:**

If the game crashes, you can use gdb to get a detailed backtrace showing exactly where the crash occurred:

```bash
cd build-debug
gdb ./bmenace1
```

At the gdb prompt, type `run` to start the game:
```
(gdb) run
```

When the game crashes, gdb will stop at the crash location. Use these commands to investigate:

```
(gdb) bt              # print a full backtrace (call stack)
(gdb) bt full         # backtrace with local variable values
(gdb) frame N         # switch to stack frame N (from the backtrace)
(gdb) print varname   # print the value of a variable
(gdb) info locals     # print all local variables in the current frame
(gdb) list            # show source code around the current location
```

You can also set breakpoints before running:
```
(gdb) break main          # break at the start of main()
(gdb) break ID_CA.c:650   # break at a specific file and line
(gdb) break CA_CacheMap   # break when a function is called
(gdb) run                  # start the program
(gdb) continue             # resume after hitting a breakpoint
(gdb) next                 # step over to the next line
(gdb) step                 # step into a function call
```

To get a backtrace from a core dump after the fact:
```bash
# Enable core dumps
ulimit -c unlimited

# Run the game (it will produce a 'core' file on crash)
./bmenace1

# Analyze the core dump
gdb ./bmenace1 core
(gdb) bt full
```

**Notes:**

In order to play the game, the original game data files from the freeware or shareware release are required —
this repository doesn't contain any data files.
