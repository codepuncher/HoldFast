# Nexus Mods Page Content

## Short Description

Hold the gamepad Start button to open the world map. Short press opens whatever menu Start is bound to in your controls.

---

## Tags

- Gameplay
- Interface
- Quality of Life
- Map
- SKSE
- Controllers and Gadgets

---

## Long Description (BBCode)

```bbcode
[center][size=5][b]QuickMap[/b][/size]
[i]Hold Start · Open the World Map[/i][/center]

[line]

[size=4][b]Overview[/b][/size]

Hold the gamepad [b]Start button[/b] for a configurable duration to open the [b]world map[/b] directly. A short press opens whatever menu Start is bound to in your controls.

[color=#ffcc00][b]Gamepad only.[/b][/color]

[line]

[size=4][b]Requirements[/b][/size]

[list]
[*][url=https://skse.silverlock.org/]SKSE64[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[/list]

[line]

[size=4][b]Installation[/b][/size]

[b]Mod manager (recommended):[/b]
[list=1]
[*]Install the requirements above.
[*]Install QuickMap via your mod manager.
[*]Launch Skyrim via SKSE.
[/list]

[b]Manual:[/b]
[list=1]
[*]Install the requirements above.
[*]Copy [font=Courier New]QuickMap.dll[/font] and [font=Courier New]QuickMap.ini[/font] to [font=Courier New]Data\SKSE\Plugins\[/font].
[*]Launch Skyrim via SKSE.
[/list]

[line]

[size=4][b]Configuration[/b][/size]

Edit [font=Courier New]Data\SKSE\Plugins\QuickMap.ini[/font] to adjust the hold duration (default: 0.5 seconds):

[code]
[General]
fHoldDuration=0.5
[/code]

[line]

[size=4][b]Compatibility[/b][/size]

[list]
[*]Compatible with Skyrim SE and AE.
[*]No ESP/ESL required.
[*]Compatible with any mod that does not also intercept the gamepad Start button.
[/list]

[line]

[size=4][b]Credits[/b][/size]

[list]
[*]Inspired by [b]Red Dead Redemption 2[/b]
[*][url=https://skse.silverlock.org/]SKSE[/url] by the SKSE Team
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE plugins[/url] by [url=https://www.nexusmods.com/profile/meh321]meh321[/url]
[*][url=https://github.com/alandtse/CommonLibVR]CommonLibSSE-NG[/url] by [url=https://github.com/alandtse]alandtse[/url] and contributors
[/list]
```
