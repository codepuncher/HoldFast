# QuickMap

Hold a configurable gamepad button (**Start** or **Back**, default: **Start**) for a configurable duration to open the **map**. A short press opens the button's normal menu.

> **Gamepad only.** Default behaviour is fully preserved.

Inspired by Red Dead Redemption 2's hold-Start-to-open-map mechanic.

**[NexusMods page](https://www.nexusmods.com/skyrimspecialedition/mods/180226)**

---

## Requirements

- [Skyrim Special Edition](https://store.steampowered.com/app/489830) or Anniversary Edition
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Installation

**Mod manager (recommended):**
1. Install the requirements above.
2. Install QuickMap via your mod manager.
3. Launch Skyrim via SKSE.

**Manual:**
1. Install the requirements above.
2. Copy `QuickMap.dll` and `QuickMap.ini` to `Data\SKSE\Plugins\`.
3. Launch Skyrim via SKSE.

## Configuration

Edit `Data\SKSE\Plugins\QuickMap.ini`:

```ini
[General]
; Duration in seconds the button must be held to open the map (default: 0.5)
fHoldDuration=0.5
; Button to hold for the map. Short press performs the button's normal function.
; Valid values: Start, Back (case-insensitive)
sButton=Start
```

Logs are written to:
```
%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\QuickMap.log
```

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
git clone --recurse-submodules https://github.com/codepuncher/QuickMap.git
cd QuickMap
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

The DLL lands in `build/release-linux/QuickMap.dll`.

##### Deploy to mod manager

```bash
./scripts/deploy.sh
# or directly (deploy.sh sources .env for you; cmake does not):
source .env && cmake --workflow --preset deploy
```

Copies `QuickMap.dll`, `QuickMap.pdb`, and `QuickMap.ini` into `$SKYRIM_MODS_FOLDER/QuickMap/SKSE/Plugins/`.

#### Windows (MSVC)

```bash
cmake --preset release-windows
cmake --build --preset release-windows
```

The DLL lands in `build/msvc/Release/QuickMap.dll`.

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

## Credits

- Inspired by the hold-Start-to-open-map mechanic in **Red Dead Redemption 2**
- [SKSE](https://skse.silverlock.org/) by the SKSE Team
- [Address Library for SKSE plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) by [meh321](https://www.nexusmods.com/profile/meh321)
- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR) by [alandtse](https://github.com/alandtse) and contributors

---

## License

MIT — see [LICENSE](LICENSE).
