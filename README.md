# CppNetworkingModule
# CppNetworkingModule

A **Windows-only C++23 networking module** for a game engine.

The project builds **two executables**:
- **ServerApp** — must be started first
- **ClientApp** — connects to the server

This project uses the **Windows WinSock API** and **will not work outside native Windows**.

---

## READ THIS FIRST (Important)

If **any** of the following are true, the project will **not build**:
- You are on Linux, macOS, or WSL
- You are trying to cross-compile
- You do not have a Windows C++ compiler installed
- You skipped installing C++ build tools in Visual Studio

This is intentional.

---

## Supported Platforms

### Operating System
- Windows 10 (64-bit)
- Windows 11 (64-bit)

### Supported Compilers / Toolchains
- MSVC (Visual Studio)
- clang-cl (Visual Studio + LLVM)
- MinGW-w64 (GCC for Windows)

### Explicitly Unsupported
- WSL (any version)
- Linux
- macOS

Reason: **WinSock is Windows-only** and is unavailable in non-native environments.

---

## Before You Build (Sanity Checklist)

Verify **all** of the following before proceeding:

- `winver` shows Windows 10 or 11
- `git --version` works
- One of the following works:
  - `cl` (MSVC)
  - `clang-cl`
  - `g++`
- `premake5 --version` works  
  **OR** `vendor/premake/premake5.exe` exists
- You are running commands from a **normal Windows terminal**
  - Command Prompt
  - PowerShell
  - Developer Command Prompt

If any item fails, fix it **before** continuing.

---

## Required Software

### Always Required
- Git
- Premake 5

Premake can be:
- Installed globally and available in `PATH`, **or**
- Placed at:  
  `vendor/premake/premake5.exe`

> All commands below assume **vendor/premake/premake5.exe**.  
> If you use a global Premake, replace the path with `premake5`.

---

### Toolchain Requirements

#### MSVC (Recommended)
- Visual Studio 2022 or newer
- Workload: **Desktop development with C++**

#### clang-cl
- Visual Studio (same requirements as MSVC)
- LLVM installed with `clang-cl` available

#### MinGW-w64
- MinGW-w64 installed
- `g++` available in `PATH`

#### Ninja (Optional)
- Ninja installed and available in `PATH`
- Still requires a compiler (MSVC or clang-cl)

---

## Getting the Source

Clone the repository and enter it:

git clone https://github.com/Majid-Shahani/CppNetworkingModule

cd CppNetworkingModule

---

## Build Overview

You must choose **exactly one** build path below.

All build paths:
- Build **both** ServerApp and ClientApp
- Output binaries to the same directory
- Target **Debug / x64**

---

## Build Path 1 — MSVC (Visual Studio)

Uses:
- Compiler: MSVC
- Build system: Visual Studio / MSBuild

Generate project files:

call vendor\premake\premake5.exe vs2026

Build (Debug, x64):

msbuild Networking.slnx /p:Configuration=Debug /p:Platform=x64 /m

Run the server:

bin\Windowsx64-Debug\ServerApp\ServerApp.exe

In a **second terminal**, run the client:

bin\Windowsx64-Debug\ClientApp\ClientApp.exe


---

## Build Path 2 — clang-cl (Windows)

Uses:
- Compiler: clang-cl
- Build system: Visual Studio / MSBuild

Generate project files:

call vendor\premake\premake5.exe vs2026 --cc=clang


Build (Debug, x64):

msbuild Networking.sln /p:Configuration=Debug /p:Platform=x64 /m


Run the server:

bin\Windowsx64-Debug\ServerApp\ServerApp.exe


In a **second terminal**, run the client:

bin\Windowsx64-Debug\ClientApp\ClientApp.exe


---

## Build Path 3 — Ninja

Uses:
- Build system: Ninja
- Compiler: MSVC or clang-cl (must already be configured)

Generate Ninja files:

call vendor\premake\premake5.exe ninja

Build everything:

ninja


Run the server:

bin\Windowsx64-Debug\ServerApp\ServerApp.exe

In a **second terminal**, run the client:

bin\Windowsx64-Debug\ClientApp\ClientApp.exe


---

## Build Path 4 — GNU Make (MinGW-w64)

Uses:
- Compiler: GCC (MinGW-w64)
- Build system: GNU Make

Generate Makefiles:

call vendor\premake\premake5.exe gmake2


Build (Debug, x64):

make config=Debug_x64


Run the server:

bin/Windowsx64-Debug/ServerApp/ServerApp.exe


In a **second terminal**, run the client:

bin/Windowsx64-Debug/ClientApp/ClientApp.exe


---

## Output Directory

All builds output to:

bin/Windowsx64-Debug/


Contents:

- `ServerApp/ServerApp.exe`
- `ClientApp/ClientApp.exe`

If this directory does not exist, the build failed.

---

## Correct Run Order (Mandatory)

1. Start **ServerApp**
2. Start **ClientApp** in a separate terminal

If the server is not running, the client will fail.

---

## Common Failure Cases

- Running from WSL → **Will not configure**
- Missing Visual Studio C++ workload → **Will not compile**
- Wrong terminal (no compiler in PATH) → **Build errors**
- Running ClientApp first → **Connection failure**

These are not bugs.

---

## Notes

- Language standard: **C++23**
- Platform: **Native Windows only**
- Cross-platform support is not planned at this time
- Failures on unsupported platforms are expected

---
