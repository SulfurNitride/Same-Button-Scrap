# Scrap With Same Button

Scrap With Same Button lets you press the scrap button a second time to confirm scrapping in Fallout 4.

With the default keyboard controls:

```text
R — open the scrap confirmation
R — confirm
```

The normal E, Enter, mouse, and controller confirmation controls continue to work.

The optional instant mode removes the confirmation and scraps on the first press. An independent material-summary option reports recovered components through Fallout's native HUD messages.

## Key bindings

The plugin follows Fallout 4's logical `XButton` action, which is the scrap action in workshop and workbench contexts. It does not hardcode R:

- Rebinding the scrap action changes which button can be pressed twice.
- Rebinding R to a different action does not make that action confirm scraps.
- The physical confirm binding is irrelevant. The plugin follows Fallout's native disabled `Activate` release path, while the player's normal confirm keys still work.

## Supported scrapping

- Settlement and workshop world objects
- Weapons and armor at workbenches

The plugin identifies native scrap confirmation types. It does not match English message text and does not alter unrelated confirmation boxes.

## Configuration

The FOMOD installer provides four presets: same-button confirmation, same-button confirmation with material summaries, instant scrap, and instant scrap with material summaries. Same-button confirmation without notifications is the safe default.

Settings are stored in `Data/F4SE/Plugins/ScrapWithSameButton.ini`:

```ini
[Scrapping]
Mode=SameButton
ShowMaterials=false
```

`Mode` accepts `SameButton` or `Instant`. Instant mode is intentionally opt-in because one button press permanently scraps the selected object. `ShowMaterials` accepts `true` or `false` and uses the native HUD notification area, allowing HUD replacers to control its placement and appearance.

## Requirements

- Fallout 4 Script Extender (F4SE)
- Address Library for F4SE Plugins

Choose the FOMOD option matching `Fallout4.exe`:

| Option | Fallout 4 version |
| --- | --- |
| OG | 1.10.163 |
| NG | 1.10.980 or 1.10.984 |
| AE | 1.11.137, 1.11.159, 1.11.169, 1.11.191, or 1.11.221 |

## Compatibility

The plugin does not replace `MessageBoxMenu.swf` or `ExamineConfirmMenu.swf`, so it is designed to coexist with interface replacers such as FallUI Confirm Boxes. A replacer that bypasses Fallout's native `ExamineConfirmMenu` flow may prevent the optional instant mode from hiding its confirmation.
