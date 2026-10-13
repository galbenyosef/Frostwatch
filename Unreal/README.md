# Native Unreal source

This is the project-authored C++ source snapshot for Frostwatch 0.5.10, built with Unreal Engine **5.8.2** on Windows. The running game uses this native module. Godot is not required.

`Source/Frostwatch/FWGame.cpp` integrates movement, combat contact, enemy responses and campaign state. `FWFighting.cpp` implements directional move selection, anatomical wound selection, injury effects and bounded blood animation. `FWCombatEffects.cpp` handles matching detached parts, wounds and encounter cleanup; `FWCombatAuthor.cpp` bakes the detached meshes in the editor. `FWCombatContactTests.cpp` contains the actual in-world combat regression suite. `FWMenus.cpp` includes the fighting guide and remappable controls.

The source snapshot excludes licensed source art, cooked game data, the authored Hearthmere map, engine binaries, personal settings and saves. **It is not a self-contained clone-and-play project.** Use the packaged [Windows download](https://github.com/JonBoyd2401/Frostwatch/releases/tag/v0.5.10-preview) to play. Compiling the native module requires the matching Unreal installation and Windows C++ toolchain; recreating the complete game also requires the separately held, correctly licensed content library and campaign data.

For an authorized full project checkout, copy `Config/DefaultEngine.template.ini` to `Config/DefaultEngine.ini`, generate project files and build `FrostwatchEditor` / `Frostwatch` for Win64 Development. `Scripts/author_fighting.py` ensures the 20 matching body-part assets and generates the droplet material from the four character meshes in the current Hearthmere map. It saves those assets without saving or altering the map's scenery. The included PowerShell verification scripts describe the isolated Unreal test entry points; adapt local executable paths for your installation.

`source-manifest.json` records SHA-256 hashes of the exported code/configuration files. No rights to third-party engine code or source assets are granted by this snapshot. Existing project and contributor rights remain reserved; see [distribution terms](../DISTRIBUTION.md) and [game license](../GAME-LICENSE.md).
