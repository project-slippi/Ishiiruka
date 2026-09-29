# Rectangle (Melee) keyboard controls

The GameCube controller configuration has a **Rectangle (Melee)** group. When it is enabled, the pad is
played like a B0XX-style rectangle controller: digital Up/Down/Left/Right, Mod X/Mod Y and C-buttons
are turned into control stick and C-stick coordinates in code, and the regular Control Stick and C Stick
groups are ignored. A/B/X/Y/Z/Start, L/R and the D-Pad still come from their usual groups.

With **Controller Ruleset Limits** on (the default), output follows the
[Melee Controller Ruleset](https://docs.google.com/document/d/1abMqoatAGh_ZhQD1qJaQx6YqFAppCjU5KyF3mgvDQVw)
rules for controllers that replace the control stick with buttons, using a port of the compliant
[HayBox](https://github.com/CarVac/HayBox) firmware logic:

- Neutral SOCD on both axes of both sticks (Left + Right = neutral).
- Travel time: 6-8 ms to rim cardinals and diagonals, 12 ms to anything else, including back to neutral.
- Output fuzzing: non-neutral axes land up to 1 unit off target (25/50/25), re-rolled only when the
  input changes.
- L/R lockout: angles closer than 27 degrees to an axis are pushed out to 27 degrees while L or R is held.
- Timing lockouts: burst tap/quarter-circle SDI, crouch to up-tilt in under 3 frames, pivot tilts within
  8 frames, and diagonal dash pivot tilts.
- Modifier coordinates, firefox angles, angled forward smash and the Mod X + Mod Y D-pad layer come
  from HayBox's ruleset-compliant Melee mode.

Turning the limits off gives instant coordinates with second-input-priority SOCD, which is useful for
practice but is not ruleset compliant. **Crouch Walk Option Select** switches the down diagonals to a
crouch-walk-safe coordinate.

Timing uses emulated time. Keyboard state is sampled whenever Dolphin refreshes input (about four times
per frame) and again whenever the game polls the controller.

## Default keyboard layout

Pressing **Default** in the controller configuration (or starting without a `GCPadNew.ini`) binds the
keyboard to the community B0XX keyboard layout, the same one used by
[b0xx-ahk](https://github.com/agirardeau/b0xx-ahk). The same bindings are available as the
`Keyboard_B0XX` profiles (`_macOS` and `_Linux` variants included).

| Key | Input | Key | Input |
|-----|-------|-----|-------|
| `Q` | L | `9` | R |
| `2` | Left | `0` | Y |
| `3` | Down | `-` | Lightshield |
| `4` | Right | `=` | Midshield |
| `V` | Mod X | `O` | B |
| `B` | Mod Y | `P` | X |
| `7` | Start | `[` | Z |
| `M` | A | `]` | Up |
| `N` | C-Left | `,` | C-Right |
| `K` | C-Up | `Space` | C-Down |
| Arrow keys | D-Pad | | |
