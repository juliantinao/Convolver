# Icon update: LogSweepGenerator and Convolver

Make the app icons match the ones now live on acoustics.juliantinao.com: the ADM icon's
near-black background with a colored wave, one color per app.

| App | Disc (background) | Wave |
|---|---|---|
| LogSweepGenerator | `#0D1117` | `#63B3ED` (blue) |
| Convolver | `#0D1117` | `#F08A6C` (coral) |

These are the ADM icon's own background and harmonic colors. The geometry (disc, three-cycle
wave, stroke width) does not change.

The icon has a single source in each repo: `tools/icons/make-app-icon.ps1`. It generates the three
committed assets in `packaging/icon/` (`appicon.svg`, `appicon.png`, `appicon.ico`), which feed the
window/taskbar icon (`AppIcon.h`), the `.exe` icon (CMake `ICON_BIG`) and the installer icon
(Inno Setup). Never edit those three files or `AppIcon.h` by hand.

> **Gotcha:** the raster renderer (PNG + ICO) does **not** use `$waveHex`. It hardcodes
> `[System.Drawing.Color]::White` for the pen. If you only change `$waveHex`, the SVG turns
> colored but the `.exe` and installer icons stay white. Step 2 fixes that.

---

## Steps (repeat in each repo)

Repos: `D:\Codigos\LogSweepGenerator` and `D:\Codigos\Convolver`. The script is identical in both
except for the app name, so the edits are the same, only the wave color differs.

### 1. Branch

```powershell
git switch -c icon-colors
```

### 2. Edit `tools/icons/make-app-icon.ps1`

**a) The `design` block (around line 126).** Replace:

```powershell
$disc    = [System.Drawing.Color]::FromArgb(255, 0x3A, 0x3A, 0x5C)
$discHex = '#3A3A5C'
$waveHex = '#FFFFFF'
```

LogSweepGenerator:

```powershell
$disc    = [System.Drawing.Color]::FromArgb(255, 0x0D, 0x11, 0x17)
$discHex = '#0D1117'
$wave    = [System.Drawing.Color]::FromArgb(255, 0x63, 0xB3, 0xED)
$waveHex = '#63B3ED'
```

Convolver:

```powershell
$disc    = [System.Drawing.Color]::FromArgb(255, 0x0D, 0x11, 0x17)
$discHex = '#0D1117'
$wave    = [System.Drawing.Color]::FromArgb(255, 0xF0, 0x8A, 0x6C)
$waveHex = '#F08A6C'
```

**b) The pen in `New-IconBitmap` (around line 165).** Replace:

```powershell
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), ([single](1.8 * ($canvas / 32.0)))
```

with:

```powershell
$pen = New-Object System.Drawing.Pen ($wave), ([single](1.8 * ($canvas / 32.0)))
```

**c) The description in the header comment (around line 45).** Replace:

```
Source/AppIcon.h: a dark blue-grey disc with a white three-cycle sine wave
```

LogSweepGenerator: `... a near-black disc (#0D1117, the ADM icon background) with a blue (#63B3ED) three-cycle sine wave`

Convolver: `... a near-black disc (#0D1117, the ADM icon background) with a coral (#F08A6C) three-cycle sine wave`

The comment may wrap onto the next line. Keep the rest of the sentence as it is.

### 3. Regenerate the assets

From the repo root:

```powershell
.\tools\icons\make-app-icon.ps1
```

It rewrites `packaging/icon/appicon.svg`, `appicon.png` and `appicon.ico` (only files whose bytes change).

### 4. Check the result

```powershell
Select-String -Path packaging\icon\appicon.svg -Pattern 'fill="#|stroke="#'
```

You should see `fill="#0D1117"` and the app's wave color. Then open `packaging\icon\appicon.png`
and confirm the wave is colored, not white. That proves step 2b took effect.

### 5. Rebuild the app and the installer

The `.exe` icon and the window icon are compiled in, so a rebuild is required:

```powershell
cmake --build --preset release
.\tools\dist\build-installer.ps1
```

Install it and check the icon in the title bar, the taskbar, Explorer and the installer itself.
If Windows still shows the old icon, that's its icon cache. Run `ie4uinit.exe -show` or sign out
and back in.

### 6. Commit and merge

```powershell
git add tools/icons/make-app-icon.ps1 packaging/icon/
git commit -m "Recolor app icon: wave on the ADM icon background"
git switch main
git merge --ff-only icon-colors
git push origin main
```

---

## Publishing the new installers on the website

The website already shows the new icons. To ship the recolored apps as downloads, follow the
normal release flow from the juliantinao-com README:

1. In each app repo, bump the patch version in `CMakeLists.txt` (for example LogSweepGenerator
   0.1.1 → 0.1.2, Convolver 0.2.0 → 0.2.1), so the installer gets a new file name and visitors
   don't get a cached copy. Then rebuild the app and the installer (step 5).
2. In `D:\Codigos\juliantinao-com`, on a branch:
   ```powershell
   npm run sync-downloads
   ```
   This reads the CMake versions, copies the installers and updates `src/data/releases.json`.
3. Commit, merge to `main`, then run `npm run deploy`.

## Optional: ADM

ADM already uses this palette (its icon is the source of these colors). No change is needed there.
