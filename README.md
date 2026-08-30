# NetXbox

A lightweight, self-contained **homebrew web browser** for **Windows** and the **Xbox 360** (XDK / J-Runner homebrew scene). NetXbox renders pages with its own small HTML layout engine and exposes a gamepad-first interface, so the whole browser can be driven from an Xbox 360 controller.

NetXbox does **not** depend on WebKit/Blink or any embedded Chromium. It ships with its own HTML parser, CSS box layout, bitmap font renderer, and network stack, which keeps the binary tiny and lets the same core code run on both a desktop PC and an Xbox 360 console.

---

## Features

- **Cross-platform core** — one shared codebase (C11) targets Windows (Win32/WinHTTP + OpenGL/D3D renderer front-end) and the Xbox 360 (XDK D3D9 / XUI).
- **Built-in HTML engine** — custom parser, box layout, and rendering pipeline (see `src/html`, `src/browser`, `src/renderer`).
- **Gamepad-first controls** — full navigation, tabs, refresh, back/forward, and an on-screen keyboard enterable with the controller.
- **On-screen keyboard** — friendly gamepad-driven keyboard for both the URL bar and web search.
- **Tabs** — switch between pages with LB/RB.
- **Ad-block lists** (`src/adblock`) and **page translation** support (`src/core/translate`).
- **Mojeek search** — the default search provider (chosen because it works without JavaScript, which the gamepad client does not run).

---

## Repository layout

```
NetXbox/
├── include/            # Public headers (+ xbox_compat shims)
├── src/
│   ├── app.c           # App loop, HUD, input, OS keyboard
│   ├── main.c          # Entry point
│   ├── browser/        # Tab manager, navigation, redirects
│   ├── html/           # HTML parser + layout
│   ├── network/        # HTTP client (WinHTTP / raw socket)
│   ├── renderer/       # Box/text/image rendering
│   ├── graphics/       # Font, framebuffer, image decode
│   ├── ui/             # UI widgets (+ XUI skin for Xbox)
│   ├── adblock/        # Ad-blocking
│   ├── core/           # Types, translation, logo data
│   └── platform/       # win32/ and xbox360/ platform layers
├── resources/          # Windows resources + icons
├── shaders/            # Compiled + source shaders
├── cmake/              # CMake toolchain support
├── thirdparty/         # Vendored third-party code (excluded from VCS)
└── build*.bat          # One-click build scripts
```

---

## Prerequisites

| Target | Requirements |
| ------ | ------------ |
| **Windows** | A 64-bit PC, **CMake** ≥ 3.10, and a C11 compiler + toolchain (Visual Studio Build Tools / cl.exe on `PATH`, or MinGW-w64). |
| **Xbox 360** | The **Microsoft Xbox 360 SDK (XDK)** installed at `C:\Program Files (x86)\Microsoft Xbox 360 SDK` (the `cl.exe`/`link.exe`/`imagexex.exe` toolchain and Xbox libraries are used directly). |

> **Note:** `thirdparty/` is excluded from the repository. If sources there are required for builds outside this repo, restore them into that folder before building.

---

## Building for Windows

### Option A — One-click script

1. Ensure `cmake` (and your MSVC/MinGW toolchain) is on your command line `PATH`.
2. Run `build.bat` from the repository root:

```bat
build.bat
```

This configures a CMake `build/` directory with `BUILD_XBOX360=OFF`, builds the **Release** configuration, and launches `build\Release\netxbox.exe` on success.

### Option B — Manual CMake

```bat
cmake -S . -B build -DBUILD_XBOX360=OFF
cmake --build build --config Release
```

The output executable is `build\Release\netxbox.exe`. Just double-click it or run it from a terminal to launch the browser.

---

## Building for Xbox 360

> You need the **Microsoft Xbox 360 SDK (XDK)** installed. NetXbox targets a dev-kit/RGH homebrew environment.

### Option A — One-click script

Run `build_xbox360.bat` from the repository root:

```bat
build_xbox360.bat
```

This:
1. Compiles every translation unit with the XDK compiler (`/D_XBOX /DPLATFORM_XBOX360`).
2. Links against the Xbox libraries (`d3d9`, `xinput2`, `xgraphics`, `xuirun`, `xnet`, `xboxkrnl`, …).
3. Converts the PE to an **XEX** with `imagexex`.
4. Builds the XUI media package (`netxbox.xzp`), the keyboard/skin/toolbar/settings/home skins, and stages media (`cursor.png`, `arialuni.ttf`, `netxbox.xzp`) into `build_xbox\Release\media\`.

On success you get:

```
build_xbox\Release\netxbox.xex
```

### Option B — Manual CMake (XDK toolchain)

```bat
cmake -S . -B build_xbox -DBUILD_XBOX360=ON
cmake --build build_xbox --config Release
```

`CMakeLists.txt` auto-detects the XDK at `C:/Program Files (x86)/Microsoft Xbox 360 SDK` (override with `-DXEDK=...`) and runs `imagexex` in a post-build step to produce the `.xex`.

### Deploying to a console

1. Copy `build_xbox\Release\netxbox.xex` to a USB drive (FAT32/FATX).
2. Launch it from **XeXMenu** (or any homebrew file loader) on your Xbox 360.
3. The `media/` folder (XUI skin package + fonts) must sit next to the `.xex`.

---

## Controls (gamepad)

| Button | Action |
| ------ | ------ |
| **A** | Activate / click / select |
| **B** | Back / close settings |
| **X** | Open search on-screen keyboard |
| **Y** | Refresh page |
| **START** | Open URL keyboard |
| **BACK** | Open settings |
| **LB / RB** | Switch tabs |

**While an on-screen keyboard is open:**

| Button | Action |
| ------ | ------ |
| **A / tap** | Type the selected character |
| **Y** | Space |
| **X** | Delete word before the cursor |
| **LB / RB** | Move cursor left / right |
| **ENTER** | Apply the typed text |
| **B** | Close the keyboard |

---

## Notes

- **Search** uses Mojeek (`https://www.mojeek.com/search?q=`), which serves usable pages without JavaScript. DuckDuckGo was used earlier but blocks the JS-less client.
- The build includes an **on-screen debug HUD** (state, HTTP status, body length, gzip/chunked flags, parser node counts) shown on non-home tabs, useful while diagnosing rendering issues.
- The HTTP client does **not** send `Accept-Encoding: gzip` on the Xbox path (the console cannot decompress), so keep that in mind if you modify the network stack.

---

## License

This project is a homebrew development effort. See the repository for license details.
