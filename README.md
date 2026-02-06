# L1GHTER

L1GHTER is a professional-grade clip recorder optimized for Roblox. It records the last X seconds in the background with a low-latency capture pipeline, shows tasteful overlay notifications, and saves clips as MP4 (H.264). It is anti-cheat safe by design: no DLL injection, no memory hooks, and no kernel drivers.

## Features
- Rolling background recording buffer (default 30 seconds, configurable).
- Global hotkey to save the last X seconds.
- RobloxPlayerBeta.exe process detection and automatic start/stop.
- Desktop Duplication capture (DirectX 11) with FPS matching.
- Hardware-accelerated H.264 encoding via Media Foundation hardware MFTs (NVENC/AMF/QSV), with fallback.
- WASAPI system audio + optional microphone capture.
- Anti-cheat safe overlay as a separate transparent D3D11 window.
- System tray UI + dark-mode desktop window.
- Robust logging and graceful shutdown.

## Build (MSVC)
1. Install Visual Studio 2022 with **Desktop development with C++**.
2. Ensure Windows SDK 10.0.19041+ is installed.
3. Configure and build:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Run `build/Release/L1GHTER.exe`.

## Runtime notes
- Output directory defaults to `Videos/L1GHTER`.
- Hotkey default: `Ctrl + Shift + S`.
- Logs: `logs/lighter.log` next to the executable.

## Architecture overview

### Desktop Duplication capture pipeline
L1GHTER uses the DXGI Desktop Duplication API to capture frames from the primary output (DXGI output 0). Each frame is retrieved via `AcquireNextFrame`, which provides a GPU texture. Frames are passed to a Media Foundation H.264 encoder as DXGI surface buffers for low-copy GPU-side encoding.

### Rolling buffer implementation
Encoded video samples are stored in a time-based ring buffer. The buffer automatically trims samples older than the configured clip duration (default 30 seconds). When the user presses the hotkey, the buffer is snapped and written to an MP4 file.

### Hardware encoder selection & fallback
Media Foundation enumerates video encoder MFTs with `MFT_ENUM_FLAG_HARDWARE`, preferring hardware-backed encoders (NVENC/AMF/QSV). If none are available, L1GHTER falls back to a software H.264 encoder. Encoding input is NV12, output H.264.

### WASAPI audio sync
System audio is captured using WASAPI loopback in shared mode. Microphone capture is optional; if enabled and the audio formats are compatible, audio is mixed in software. Samples are timestamped in 100ns units and kept in a ring buffer synced to the capture timeline.

### Overlay window architecture (anti-cheat safe)
L1GHTER creates a separate transparent, click-through, always-on-top window aligned to the Roblox window. It uses DirectX 11 + Direct2D to draw small notifications ("L1GHTER started" and "Clip saved"). The overlay never injects into Roblox and never reads or writes the game process memory.

### Roblox process and window tracking
A polling process watcher detects `RobloxPlayerBeta.exe`. When found, the recorder initializes capture and the overlay aligns to the Roblox window. When the process exits, capture is stopped and resources are released.

## Performance tuning notes
- Capture FPS is locked to the configured target (default 60) to minimize overhead.
- Duplicate output timeout is kept short to avoid long stalls.
- H.264 bitrate scales with resolution and FPS; it can be tuned in `encoder.cpp` for quality/perf tradeoffs.
- Capture pauses when the Roblox window is minimized to avoid unnecessary work.

## Anti-cheat & safety considerations
- No DLL injection or memory hooks.
- No kernel drivers.
- Overlay is a separate window with transparent, click-through styles.
- Uses documented Windows APIs only.

## Future upgrades
- DX12 capture: add a WGC (Windows Graphics Capture) backend for DX12-only games.
- AV1 encoding: add AV1 MFT support (Intel/AMD/NVIDIA hardware).
- Overlay themes and animations: expand Direct2D drawing with configurable themes, easing animations, and layout options.

## Packaging
For a signed installer, use WiX or MSIX:
- Build the Release binary.
- Sign with a code-signing certificate using `signtool`.
- Package with WiX Toolset or create an MSIX package with the Windows Packaging Project.

