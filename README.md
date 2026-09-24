# PencilBridge

PencilBridge turns an iPad + Apple Pencil into a LAN input surface for a selected Windows application.

The first prototype intentionally focuses on the core path:

- Native Win32 host with a target-window picker.
- Dependency-free HTTP + WebSocket server on port 8765.
- iPad Safari Pointer Events.
- Apple Pencil position, pressure, and tilt injected through the Windows synthetic pen API (PT_PEN).
- Optional one-finger input mapped to the Windows mouse.
- Absolute mapping from the iPad surface to the selected window's client area.

There is no screen streaming yet. The goal of this version is to validate that Pencil input reaches apps such as Unreal, Blender, Paint, etc. as useful Windows pen input before adding capture/video.

## Requirements

- Windows 10 version 1809 or newer.
- CMake 3.24+.
- A Visual Studio C++ toolchain.
- Ninja is optional; Build.ps1 uses it automatically when installed.
- iPad and desktop on the same LAN.

## Build

~~~powershell
.\Build.ps1
~~~

Clean rebuild:

~~~powershell
.\Build.ps1 -Clean
~~~

## Run

~~~powershell
.\Run.ps1
~~~

The desktop window shows an address similar to:

~~~text
http://192.168.1.42:8765
~~~

Open that address in Safari on the iPad.

1. Pick the target application/window in PencilBridge on the desktop.
2. Open the displayed URL on the iPad.
3. Draw/move with Apple Pencil in the large input area.
4. Enable **Finger mouse** on the iPad if you also want one-finger mouse control.

Windows Firewall may ask whether PencilBridge may accept connections. Allow it on the private network you are using.

## Input protocol

The webpage sends compact text WebSocket messages:

~~~text
device,phase,x,y,pressure,tiltX,tiltY,pointerId
~~~

Examples:

~~~text
p,d,0.250000,0.500000,0.720000,-8,14,23
p,m,0.251000,0.501000,0.810000,-8,14,23
p,u,0.251000,0.501000,0.000000,-8,14,23
~~~

Devices:

- p = pen
- t = touch/finger

Phases:

- d = down
- m = move
- u = up
- c = cancel
- h = pen hover

## Current limitations

- The target window must be visible. PencilBridge maps to its client rectangle in screen space.
- The host attempts to foreground the selected target on pointer-down, but Windows foreground restrictions still apply.
- Input into an elevated/admin application can be blocked by Windows integrity/UIPI rules if PencilBridge itself is not elevated.
- One browser client is handled at a time.
- There is currently no authentication or TLS. Use it only on a trusted LAN and do not expose port 8765 to the internet.
- No video/window streaming yet.
- Apple Pencil Pro-specific gestures such as squeeze/barrel rotation are not part of this first protocol.

## Next useful milestones

After the input path is validated:

1. Add configurable gesture mappings and keyboard modifiers.
2. Add per-app profiles.
3. Add a window-capture stream so the iPad can become a true display tablet.
4. Add pairing/authentication if the bridge is used outside a trusted LAN.
