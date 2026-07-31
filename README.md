# Scrap With Same Button

Scrap With Same Button lets you press the scrap button a second time to confirm scrapping in Fallout 4.

With the default keyboard controls:

```text
R — open the scrap confirmation
R — confirm
```

The normal E, Enter, mouse, and controller confirmation controls continue to work.

## Key bindings

The plugin follows Fallout 4's logical `XButton` action, which is the scrap action in workshop and workbench contexts. It does not hardcode R:

- Rebinding the scrap action changes which button can be pressed twice.
- Rebinding R to a different action does not make that action confirm scraps.
- The physical confirm binding is irrelevant. The plugin sends the logical `Accept` action directly, while the player's normal confirm keys still work.

## Supported scrapping

- Settlement and workshop world objects
- Weapons and armor at workbenches

The plugin identifies native scrap confirmation types. It does not match English message text and does not alter unrelated confirmation boxes.

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

The plugin does not replace `MessageBoxMenu.swf` or `ExamineConfirmMenu.swf`, so it is designed to coexist with interface replacers such as FallUI Confirm Boxes. A replacer that stops using Fallout's native `ExamineConfirmMenu` or logical `Accept` action will fail closed: the second scrap press will do nothing, and normal confirmation remains available.
