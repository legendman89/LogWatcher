[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://github.com/legendman89/LogWatcher/actions/workflows/build.yml/badge.svg)](https://github.com/legendman89/LogWatcher/actions/workflows/build.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![CommonLibVR: ng](https://img.shields.io/badge/CommonLibVR-ng-green.svg)](https://github.com/alandtse/CommonLibVR/tree/ng)

# Log Watcher

An SKSE plugin to monitor and analyze SKSE logs with in-game UI in real time.

## Prerequisites

Download CommonLibVR, check out the `ng` branch, then set its location in the `COMMONLIB_SSE_FOLDER` environment variable.

```powershell
git clone --recursive https://github.com/alandtse/CommonLibVR.git
cd CommonLibVR
git checkout ng
```

## Build

Run CMake from a Visual Studio x64 developer environment, or open the project in Visual Studio/Visual Studio Code with CMake support.

```powershell
cmake --preset debug
cmake --build build/debug
```
