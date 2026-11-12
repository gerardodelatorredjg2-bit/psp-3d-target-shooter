# PSP 3D Target Shooter

Native C/C++ homebrew arcade target shooter for Sony PSP, built with PSPDEV/PSPSDK.

## Features
- First-person-style 3D target range rendered with the PSP GU.
- Procedural low-poly pistol model (no external model assets or unclear third-party licenses).
- Crosshair, score, ammo, target counter and simple target-hit reactions.
- PSP controls: analog stick to aim, R to fire, START to pause, SELECT to restart, Triangle to exit.
- GitHub Actions build that produces `EBOOT.PBP`.

## Build locally
Install PSPDEV/PSPSDK and run:

```sh
make
```

The output is `EBOOT.PBP` in the project root. Copy it to `/PSP/GAME/PSP3DSHOOTER/EBOOT.PBP` on a PSP with homebrew support, or open it in PPSSPP.

## Controls
| Input | Action |
|---|---|
| Analog stick | Aim |
| R trigger | Fire |
| START | Pause/resume |
| SELECT | Restart round |
| TRIANGLE | Exit |

## Build artifact
GitHub Actions builds on pushes and pull requests. Download the `EBOOT.PBP` artifact from the workflow run.

## Notes
This is an original project. The pistol is generated from geometry in the source code, so there are no external model licensing dependencies. The initial build focuses on a complete playable foundation; visual/audio polish and performance tuning should be validated on PPSSPP and real PSP hardware.

## License
MIT. See [LICENSE](LICENSE).
