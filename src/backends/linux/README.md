# Linux backend

`display.cpp` defines the same 256 x 128, 4-bit grayscale framebuffer as the
ESP32 backend. `clearGray()`, `drawGrayBitmap()`, `drawBox()`, and `drawBitmap()`
work entirely in memory. `drawBitmap()` reads XBM-style one-bit rows; pixels
with a zero bit are preserved unless `opaque` is true.
`initDisplay()` creates a resizable SDL2 window, renderer, and streaming texture.
The renderer uses SDL's software mode so the simulator does not require a GLX
context.
SDL2_image loads `DevBoxPlayerBackground.png` from the executable directory.
The 256 x 128 framebuffer is drawn at `(6, 6)` inside the 268 x 144 background,
then SDL scales the complete image to the window size.
`sendToDisplay()` converts the framebuffer to SDL2 pixels and presents that
texture. The gradient demo handles the window close event.

`input.cpp` maps Up/Down/Left/Right to the arrow keys, A/B/X/Y to Z/X/A/S,
Select to Backspace, and Start to Enter. `buttonRaw()` matches ESP32 pull-up
semantics: `true` means released. `buttonPressed()` reports one press after the
key has stayed down for 50 ms, using `std::chrono::steady_clock` for debounce.
The demo shows the most recent key press, including its DevBox button name when
the key is mapped.

U8g2's C core provides an offscreen 1-bit mask for `drawText()`. The mask is
copied into the 4-bit framebuffer using the same font as the ESP32 backend.
After `initDisplay()`, call `setDisplayFont(u8g2_font_ncenB14_tr)` to select a
different U8g2 font; include `<u8g2.h>` for the font declarations.
The build looks for `U8g2/src/clib` beside this library. If it is elsewhere,
pass `-DU8G2_CLIB_DIR=/path/to/U8g2/src/clib` to CMake.

With SDL2 and SDL2_image development files installed, build and run the demo from the
repository root:

```sh
cmake -S . -B build/linux
cmake --build build/linux
./build/linux/devbox_linux_gradient
```

## Controls settings

The toolbar volume knob changes `setAudioVolume()` while dragging or clicking
the track. Its 3 x 5 rectangle travels from `(137, 137)` (mute) to `(169, 137)`
(full volume). Track clicks center the knob on the pointer, clamped at either
end. Volume changes take effect as the mixer prepares new audio; samples already
queued keep their previous level. The knob also works while Controls is open.
Volume currently lasts for the application session and is not saved to JSON.

The 9 x 9 power button at `(253, 135)` sends `SDL_QUIT`, just like closing the
window. The application's event loop exits normally so its cleanup can run.
It also works while Controls is open; unsaved control edits are not saved on exit.

Opening Controls pauses the application and audio and snapshots the current
bindings and the "For this game only" checkbox.

Assigning a key already used by another button swaps the two bindings. The
status line shows `Swapped with <button>` until the next selection. Save and
Revert apply to both changes together.

- **Save** writes to the selected scope and closes the menu.
- **Controls** also saves and closes when the menu is open.
- **Revert** restores the opening snapshot and closes without writing settings.
- A save error leaves the menu open; details are printed in the terminal.

Settings live outside the build and simulated SD directories:

| Scope | File |
| --- | --- |
| Global | `$XDG_CONFIG_HOME/DevBox/settings.json` |
| Game | `$XDG_CONFIG_HOME/DevBox/games/<game-id>.json` |

If `XDG_CONFIG_HOME` is unset, empty, or relative, the base directory is
`$HOME/.config`. The Engine's `build_game.py` supplies `DEVBOX_GAME_ID` using
the project folder name. All executables in that project share it, including
level executables. Rebuilding or moving the output directory preserves settings;
renaming the project gives it a new ID. Projects with the same folder name share
an ID. Custom builds can define `DEVBOX_GAME_ID` as a string literal themselves.
Unsafe filename bytes in the ID are percent-encoded.

`input_config.cpp` loads built-in bindings, then global bindings, then game
overrides once at startup. For example, a game file can contain:

```json
{
  "controls": {
    "Up": "W",
    "Down": "S"
  }
}
```

Values are SDL scancode names. Game files store only bindings that differ from
the global settings; other bindings continue to inherit global changes. An
empty `controls` object retains the checked game scope. Saving with the checkbox
unchecked writes global bindings and removes this game's `controls` object.
Other settings fields in both files are preserved.

Each file is replaced through a temporary file and rename. Changing from game
to global scope updates two files: if removing the game override fails after the
global save, the terminal reports the partial save and the menu stays open for
retry. Revert only restores the in-memory snapshot; it cannot undo that disk write.
Malformed settings are reported and skipped on load, and saving refuses to
overwrite them. JSON parsing uses the vendored PicoJSON v1.3.0 header in `detail/`
(upstream: https://github.com/kazuho/picojson, license included in the header).

## SD folder

The Linux SD backend uses a normal directory. `initSD()` checks
`DEVBOX_SD_ROOT`, or `./sdcard` when that variable is unset. It returns false
if the directory does not exist. After initialization, `sdPath()` maps
`/sdcard/...` to that directory; other paths pass through unchanged.

From the repository root, run the file-reading demo with:

```sh
DEVBOX_SD_ROOT=examples/linux_sd/sdcard ./build/linux/devbox_linux_sd_demo
```

The demo reads `/sdcard/hello.txt` through `sdPath()`. Application file reads
will need to call the resolver too; `initSD()` does not change how the host
operating system interprets absolute paths.
