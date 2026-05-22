# Nexus Mods Page Content

## Short Description

Hold a gamepad button (Start or Back) to open a menu directly — map, journal, quests, stats or system page. Each button is independently configurable. Short press still works normally.

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
[i]Hold a gamepad button · Open the Map or Journal — fully configurable.[/i][/center]

[line]

[size=4][b]Overview[/b][/size]

Hold a gamepad button for a configurable duration to jump directly to a menu. Each button's action is independently configurable. A short press opens the button's normal menu as usual.

[b]Defaults:[/b]
[list]
[*]Hold [b]Start[/b] → open the [b]Map[/b]
[*]Hold [b]Back[/b] → open the [b]Journal System page[/b]
[/list]

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

Edit [font=Courier New]Data\SKSE\Plugins\QuickMap.ini[/font]:

[code]
[General]
; Duration in seconds a button must be held to trigger its long-press action (default: 0.5, max: 5.0)
fHoldDuration=0.5
; Long-press action for the Start (Menu) button. Short press performs the button's normal function.
; Valid values: Map, System, Quests, Stats, None (case-insensitive)
sButtonStartAction=Map
; Long-press action for the Back (View) button. Short press performs the button's normal function.
; Valid values: Map, System, Quests, Stats, None (case-insensitive)
sButtonBackAction=System
[/code]

[b]Valid actions:[/b]
[list]
[*][b]Map[/b] — opens the map
[*][b]System[/b] — opens the Journal on the System tab
[*][b]Quests[/b] — opens the Journal on the Quests tab
[*][b]Stats[/b] — opens the Journal on the Stats tab
[*][b]None[/b] — button not intercepted
[/list]

[line]

[size=4][b]Compatibility[/b][/size]

[list]
[*]Compatible with Skyrim SE and AE.
[*]No ESP/ESL required.
[*]Compatible with any mod that does not also intercept the configured gamepad button.
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
