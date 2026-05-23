# Nexus Mods Page Content

## Short Description

Hold a gamepad button (Start or Back) for a configurable duration to trigger a custom action — map, journal, quests, stats or system page. Each button is independently configurable. Short press still works normally.

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
[center][size=5][b]HoldFast[/b][/size]
[i]Hold it right there! · Configurable hold actions for gamepad[/i][/center]

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

<!-- generated:start -->

[size=4][b][color=#B8953E]Requirements[/color][/b][/size]

[list]
[*][url=https://store.steampowered.com/app/489830]Skyrim Special Edition[/url] or Anniversary Edition
[*][url=https://skse.silverlock.org/]SKSE64[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[/list]

[line]

[size=4][b][color=#B8953E]Installation[/color][/b][/size]

[b]Mod manager (recommended):[/b]
[list=1]
[*]Install the requirements above.
[*]Install HoldFast via your mod manager.
[*]Launch Skyrim via SKSE.
[/list]

[b]Manual:[/b]
[list=1]
[*]Install the requirements above.
[*]Copy [font=Courier New]HoldFast.dll[/font] and [font=Courier New]HoldFast.ini[/font] to [font=Courier New]Data\SKSE\Plugins\[/font].
[*]Launch Skyrim via SKSE.
[/list]

[line]

[size=4][b][color=#B8953E]Configuration[/color][/b][/size]

Edit [font=Courier New]Data\SKSE\Plugins\HoldFast.ini[/font]:

[code]
[General]
; Duration in seconds a button must be held to trigger its long-press action (default: 0.5, max: 5.0)
fHoldDuration=0.5
; Long-press action for the Start (Menu) button. Short press performs the button's normal function.
; Valid values: Map, System, Quests, Stats, Inventory, Magic, Favorites, TweenMenu, Wait, None (case-insensitive)
sButtonStartAction=Map
; Long-press action for the Back (View) button. Short press performs the button's normal function.
; Valid values: Map, System, Quests, Stats, Inventory, Magic, Favorites, TweenMenu, Wait, None (case-insensitive)
sButtonBackAction=System
[/code]

[b]Valid actions:[/b]

[list]
[*][font=Courier New]Map[/font] — Opens the map
[*][font=Courier New]System[/font] — Opens the Journal on the System tab
[*][font=Courier New]Quests[/font] — Opens the Journal on the Quests tab
[*][font=Courier New]Stats[/font] — Opens the Journal on the Stats tab
[*][font=Courier New]Inventory[/font] — Opens the inventory
[*][font=Courier New]Magic[/font] — Opens the magic menu
[*][font=Courier New]Favorites[/font] — Opens the favourites menu
[*][font=Courier New]TweenMenu[/font] — Opens the tween menu (Items/Magic/Map/Skills)
[*][font=Courier New]Wait[/font] — Opens the sleep/wait menu
[*][font=Courier New]None[/font] — Button not intercepted
[/list]

[b]Legacy config:[/b] If neither [font=Courier New]sButtonStartAction[/font] nor [font=Courier New]sButtonBackAction[/font] is present, the old [font=Courier New]sButton=Start[/font] (or [font=Courier New]sButton=Back[/font]) key is used as a fallback — with [font=Courier New]Map[/font] as the action. Existing INI files using [font=Courier New]sButton[/font] continue to work without changes.

Logs are written to:
[code]
%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\HoldFast.log
[/code]

<!-- generated:end -->

[line]

[size=4][b]Compatibility[/b][/size]

[list]
[*]Compatible with Skyrim SE and AE.
[*]No ESP/ESL required.
[*]Compatible with any mod that does not also intercept the configured gamepad button.
[/list]

[b]Known conflicts:[/b]
[list]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/141295]QuestJournalOverhaul[/url] — always forces the journal to the System tab, overriding HoldFast's tab selection. Fix planned.
[/list]

[line]

[size=4][b]Credits[/b][/size]

[list]
[*]Inspired by [b]Red Dead Redemption 2[/b]
[*][url=https://skse.silverlock.org/]SKSE[/url] by the SKSE Team
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE plugins[/url] by [url=https://www.nexusmods.com/profile/meh321]meh321[/url]
[*][url=https://github.com/alandtse/CommonLibVR]CommonLibSSE-NG[/url] by [url=https://github.com/alandtse]alandtse[/url] and contributors
[*][url=https://github.com/codepuncher/HoldFast]Source code: GitHub[/url]
[/list]
```
