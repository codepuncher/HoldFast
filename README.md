# HoldFast

Hold **Start** or **Back** on your gamepad for a configurable duration to open a menu directly. A short press opens the button's normal menu.

> **Gamepad only.** Default behaviour is fully preserved.

Inspired by Red Dead Redemption 2's hold-Start-to-open-map mechanic.

**[NexusMods page](https://www.nexusmods.com/skyrimspecialedition/mods/180226)**

---

<!-- nexus:start -->

## Requirements

- [Skyrim Special Edition](https://store.steampowered.com/app/489830) or Anniversary Edition
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Installation

**Mod manager (recommended):**
1. Install the requirements above.
2. Install HoldFast via your mod manager.
3. Launch Skyrim via SKSE.

**Manual:**
1. Install the requirements above.
2. Copy `HoldFast.dll` and `HoldFast.ini` to `Data\SKSE\Plugins\`.
3. Launch Skyrim via SKSE.

## Configuration

Edit `Data\SKSE\Plugins\HoldFast.ini`:

```ini
[General]
; Duration in seconds a button must be held to trigger its long-press action (default: 0.5, max: 5.0)
fHoldDuration=0.5
; Long-press action for the Start (Menu) button. Short press performs the button's normal function.
; Valid values: Map, System, Quests, Stats, Inventory, Magic, Favorites, TweenMenu, Wait, Bestiary, None (case-insensitive)
sButtonStartAction=Map
; Long-press action for the Back (View) button. Short press performs the button's normal function.
; Valid values: Map, System, Quests, Stats, Inventory, Magic, Favorites, TweenMenu, Wait, Bestiary, None (case-insensitive)
sButtonBackAction=System
```

**Valid actions:**

| Value | What it does |
|-------|-------------|
| `Map` | Opens the map |
| `System` | Opens the Journal on the System tab |
| `Quests` | Opens the Journal on the Quests tab |
| `Stats` | Opens the Journal on the Stats tab |
| `Inventory` | Opens the inventory |
| `Magic` | Opens the magic menu |
| `Favorites` | Opens the favourites menu |
| `TweenMenu` | Opens the tween menu (Items/Magic/Map/Skills) |
| `Wait` | Opens the sleep/wait menu |
| `Bestiary` | Opens The Dragonborn's Bestiary (requires mod) |
| `None` | Button not intercepted |

**Legacy config:** If neither `sButtonStartAction` nor `sButtonBackAction` is present, the old `sButton=Start` (or `sButton=Back`) key is used as a fallback — with `Map` as the action. Existing INI files using `sButton` continue to work without changes.

Logs are written to:
```
%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\HoldFast.log
```

<!-- nexus:end -->

---

## Development

### Prerequisites

#### All platforms
- [Git](https://git-scm.com/)
- [CMake](https://cmake.org/download/) 3.21+
- vcpkg — set `VCPKG_ROOT` in your environment

#### Linux
- LLVM/Clang (`clang-cl`, `lld-link`, `llvm-lib`, `llvm-rc`, `llvm-mt`)
- [xwin](https://github.com/Jake-Shadle/xwin) — downloads the Windows SDK and MSVC CRT headers/libs
- [Ninja](https://ninja-build.org/)

```bash
# Arch / CachyOS
sudo pacman -S clang lld llvm ninja

# Install xwin (requires Rust/cargo)
cargo install xwin

# Fetch Windows SDK + MSVC CRT headers to ~/.xwin (one-time, ~700 MB)
xwin splat --output ~/.xwin
```

> **Note:** On first CMake configure, `cmake/toolchains/clang-cl-cross.cmake` creates TitleCase symlinks inside your xwin installation (e.g. `Advapi32.lib → advapi32.lib`). The originals are untouched. This is required because `lld-link` is case-sensitive but CommonLibSSE-NG references libs with mixed-case names.

#### Windows
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with **Desktop development with C++**

---

### Getting Started

#### Linux

##### 1. Clone

```bash
git clone --recurse-submodules https://github.com/codepuncher/HoldFast.git
cd HoldFast
```

##### 2. Configure deploy path

Edit `.env` and set `SKYRIM_MODS_FOLDER` to your mod manager's staging folder:

```bash
# Vortex (Linux, Steam):
SKYRIM_MODS_FOLDER="$HOME/.local/share/Steam/steamapps/common/Vortex Mods/skyrimse"

# MO2 (Linux):
# SKYRIM_MODS_FOLDER="$HOME/MO2/mods"
```

##### 3. Build

```bash
./scripts/build.sh
# or directly:
cmake --preset release-linux
cmake --build --preset release-linux
```

The DLL lands in `build/release-linux/HoldFast.dll`.

##### Deploy to mod manager

```bash
./scripts/deploy.sh
# or directly (deploy.sh sources .env for you; cmake does not):
source .env && cmake --workflow --preset deploy
```

Copies `HoldFast.dll`, `HoldFast.pdb`, and `HoldFast.ini` into `$SKYRIM_MODS_FOLDER/HoldFast/SKSE/Plugins/`.

#### Windows (MSVC)

```bash
cmake --preset release-windows
cmake --build --preset release-windows
```

The DLL lands in `build/msvc/Release/HoldFast.dll`.

#### Running Tests (Windows)

Unit tests use [Catch2](https://github.com/catchorg/Catch2) and run without SKSE or Skyrim installed.

```bash
cmake --preset test-windows
cmake --build --preset test-windows
ctest --preset test-windows
```

---

### Git Hooks (Lefthook)

Prerequisites:
- `go install github.com/evilmartians/lefthook@latest`
- `clang-format` and `clang-tidy` (part of LLVM)
- `cmake-format` (`sudo pacman -S cmake-format` on Arch/CachyOS; `pip install cmakelang` elsewhere)
- `shellcheck` (`sudo pacman -S shellcheck` on Arch/CachyOS)

```bash
lefthook install
```

---

### Editor Setup (clangd / Neovim)

CMake writes `compile_commands.json` to the build directory. Symlink it to the project root:

```bash
ln -sf build/release-linux/compile_commands.json compile_commands.json
```

The `.clangd` file sets `--target=x86_64-pc-windows-msvc` so clangd resolves Windows headers correctly on Linux.

Recommended plugins: [nvim-lspconfig](https://github.com/neovim/nvim-lspconfig) with `clangd`, [clangd_extensions.nvim](https://github.com/p00f/clangd_extensions.nvim).

---

### Updating CommonLibSSE-NG

```bash
git submodule update --remote lib/commonlibsse-ng
git add lib/commonlibsse-ng
git commit -m "chore: update CommonLibSSE-NG submodule"
```

---

### CI

| Workflow | Trigger | What it does |
|---|---|---|
| `ci.yml` | PRs to `main` touching source/cmake/vcpkg | clang-format (ubuntu) → clang-tidy + build (windows, parallel) |
| `release.yml` | Push of a `v*` tag or manual `workflow_dispatch` | Builds release DLL, publishes GitHub Release; opt-in Nexus Mods upload via dispatch input |
| `lint.yml` | PRs touching `scripts/` | shellcheck on shell scripts |
| `pr-title.yml` | PR opened/edited/synchronised | Validates PR title follows Conventional Commits |

#### Nexus Mods Upload

Triggered via **workflow_dispatch** with `upload_to_nexus: true` and the `version` input set (e.g. `1.0.0`). One-time setup:

1. Upload your first file manually via the [Nexus Mods web UI](https://www.nexusmods.com) to create the file group.
2. Add to your repository:
   - **Secret** `NEXUSMODS_API_KEY` — your Nexus Mods API key
   - **Variable** `NEXUSMODS_FILE_GROUP_ID` — the file group ID

---

## Known Conflicts

- **[QuestJournalOverhaul](https://www.nexusmods.com/skyrimspecialedition/mods/141295)** — always forces the journal to the System tab, overriding HoldFast's tab selection. Fix planned.

---

## Credits

- Inspired by the hold-Start-to-open-map mechanic in **Red Dead Redemption 2**
- [SKSE](https://skse.silverlock.org/) by the SKSE Team
- [Address Library for SKSE plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) by [meh321](https://www.nexusmods.com/profile/meh321)
- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR) by [alandtse](https://github.com/alandtse) and contributors

---

## License

MIT — see [LICENSE](LICENSE).
