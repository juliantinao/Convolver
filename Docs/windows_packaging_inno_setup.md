# Empaquetado en Windows con Inno Setup

Estado: flujo implementado; verificación en máquina limpia **pendiente** — septiembre 2026
Aplica a: Convolver (JUCE GUI app, `Convolver.exe` Release de ~8.5 MB)
Complementa a: `windows_trust_and_distribution.md` (por qué empaquetamos)

Este documento es el **contrato de empaquetado**. Si vas a replicar el flujo en
otro repo JUCE, el procedimiento paso a paso está en §6.

### Qué está verificado y qué no

| | |
|---|---|
| ✅ Versión desde una sola línea de `CMakeLists.txt` hasta el nombre del instalador | Probado con un cambio real (`0.1.0` → `0.2.0`). **Depende del arreglo del `.rc` documentado en §7** |
| ✅ Los 5 lugares donde aparece el ícono | Probado, incluido el `.ico` del `.exe` (16/32/48/256) |
| ✅ El instalador compila y lleva metadata y ícono propios | Probado (0 de 1024 píxeles distintos del ícono fuente) |
| ✅ El instalador **corre desde un archivo con MOTW** (el escenario real de distribución) | Probado |
| ⏳ Instalación completa en una máquina limpia | **Pendiente** |
| ⏳ Que `Convolver.exe` instalado quede **sin MOTW** | **Pendiente** — es la premisa central de todo esto |
| ⏳ Upgrade 0.1.0 → 0.2.0 que reemplace en vez de duplicar | **Pendiente** — es la prueba del `AppId` |

No dar el flujo por bueno hasta cerrar los tres pendientes.

---

## 1. El modelo mental: dos fases con insumos distintos

| Fase | Qué necesita | Dónde vive |
|---|---|---|
| **Compilar** | Código fuente + JUCE (`C:\JUCE`) + MSVC + CMake | VS Code, dentro del repo |
| **Empaquetar** | Solo un `Release\App.exe` + Inno Setup | `tools/dist/build-installer.ps1` |

Empaquetar **no necesita el código fuente**. De ahí la separación:

```
CMakeLists.txt ──build──► Convolver.exe ──empaqueta──► Convolver-0.1.0-win64-setup.exe
   (versión)              (recurso PE)                  (dist/)
```

Y de ahí la propiedad central: **vos compilás y verificás en VS Code, y después
empaquetás ese binario exacto sin recompilar nada.** El empaquetador nunca
invoca CMake.

### Por qué el instalador resuelve el problema de SmartScreen

El instalador **extrae** `Convolver.exe` de su propio paquete y lo escribe como
archivo nuevo en la carpeta destino. Un archivo creado así **no hereda el
Mark-of-the-Web**, que es un flujo NTFS alternativo (`Zone.Identifier`) y no
parte del contenido. Resultado: el usuario sufre los avisos **una sola vez**,
al ejecutar el instalador, y la app arranca limpia para siempre.

Ver `windows_trust_and_distribution.md` §B para el detalle de qué aviso aparece
en cada momento y por qué `PrivilegesRequired=lowest` elimina el de UAC.

---

## 2. Archivos y responsabilidades

| Archivo | Rol |
|---|---|
| `packaging/app.json` | **Lo único específico de la app.** Identidad, rutas, ícono |
| `packaging/installer.iss` | Plantilla Inno **agnóstica de la app**. Todo llega por `/D` |
| `tools/dist/build-installer.ps1` | Driver: valida, absorbe la versión del `.exe`, invoca ISCC |
| `tools/icons/make-app-icon.ps1` | Genera los 3 assets de arte desde una geometría |
| `packaging/icon/appicon.{svg,png,ico}` | Assets generados (commiteados) |
| `dist/` | Salida. En `.gitignore` |

### `app.json`

```jsonc
{
  "appId": "99C8668F-3A51-4F0A-90AD-B02587B81BAA",  // GUID propio, sin llaves
  "appName": "Convolver",
  "exeName": "Convolver.exe",
  "artefactDir": "build/Convolver_artefacts/Release",
  "iconFile": "packaging/icon/appicon.ico",
  "runtimeLogName": "convolver_runtime.log",
  "licenseFile": null
}
```

**Deliberadamente ausentes: versión, publisher y nombre de producto.** Se leen
del recurso de versión del `.exe` construido (ver §4).

### El `AppId` es un GUID que inventamos nosotros

No hay autoridad que lo emita ni registro donde se declare. Inno lo usa como
clave de identidad. Dos reglas:

1. **Único por app** — si dos apps comparten GUID, se pisan.
2. **Inmutable para siempre** — si cambia entre versiones, cada upgrade se
   instala **al lado** de la anterior en vez de reemplazarla.

En `installer.iss` se escribe `AppId={{{#AppId}}`. La llave triple no es un typo:
Inno trata un `{` suelto como inicio de constante, así que hay que duplicarlo.
`{{` colapsa a `{`, luego se sustituye el GUID, y el `}` final es literal.

---

## 3. Los tres assets de arte (y por qué no alcanza uno)

Un solo script, `tools/icons/make-app-icon.ps1`, genera los tres desde una
misma definición de geometría. No pueden divergir.

| Asset | Consumidor | Formato | Por qué ese formato |
|---|---|---|---|
| `appicon.svg` | `juce_add_binary_data` | Vector | Los íconos de ventana/taskbar/diálogos se **rasterizan al tamaño exacto**. Bajar un bitmap de 512 a 16 deja el borde del disco dentado |
| `appicon.png` (512²) | `juce_add_gui_app(ICON_BIG)` | Raster | Es lo único que funciona para `ICON_BIG` (ver abajo) |
| `appicon.ico` | `SetupIconFile` | ICO multi-resolución | Inno **solo acepta `.ico`** para esta directiva |

### Por qué `ICON_BIG` no puede recibir el SVG

`juce_Icons.cpp` resuelve el archivo con `Drawable::createFromImageFile()` y
después consulta **`getWidth()`** para decidir qué resoluciones emitir
(`getBestIconForSize`, llamado con `returnNullIfNothingBigEnough = true`):

| Formato | Se convierte en | ¿Fija `Component::getWidth()`? |
|---|---|---|
| PNG | `DrawableImage` | ✅ `setImageInternal()` llama a `setBounds()` |
| SVG | `DrawableComposite` | ❌ Su constructor y `setBoundingBox()` solo llenan miembros internos. **`Component::setBounds` nunca se llama** → `getWidth()` = 0 |

Con `getWidth()` en 0, **los cuatro tamaños son descartados** y el `.ico`
generado queda **vacío**. JUCE mismo usa PNG de 512² en sus apps GUI
(`DemoRunner`, `AudioPluginHost`) y no tiene ningún `.svg` en `examples/`.

**Restricción de tiempo:** `_juce_generate_icon` llama a juceaide **durante la
configuración**, no vía `add_custom_command`. Si el asset no existe al
configurar, `_juce_check_icon_files_exist` tira `FATAL_ERROR`. Por eso los tres
archivos van **commiteados**.

**El `.ico` incluye 64×64**, que Inno recomienda y juceaide no emite (juceaide
hace 16/32/48/256).

### El generador es idempotente

`Write-FileIfChanged` no reescribe un asset cuyo contenido no cambió. No es
cosmético: el chequeo de frescura de §4 compara timestamps, y reescribir bytes
idénticos convertiría esa advertencia en ruido.

---

## 4. La versión vive en un solo lugar

```
CMakeLists.txt:3   project(Convolver VERSION 0.1.0)
                          │
                          ├─► juce_add_gui_app(VERSION "${PROJECT_VERSION}")
                          │        └─► recurso VERSIONINFO del .exe
                          │              (FileVersion / ProductVersion)
                          └─► JuceHeader.h → ProjectInfo::versionString
                                     │
        build-installer.ps1 ─────────┘  lee el .exe, NO CMakeLists.txt
                          │
                          └─► AppVersion, VersionInfoVersion,
                              nombre del instalador, "Aplicaciones instaladas"
```

**Para cambiar la versión: editás `CMakeLists.txt:3`, recompilás, empaquetás.**
Nada más. El empaquetador absorbe la versión del binario; no la declara ni la
compara contra CMake.

Consecuencia útil: el resultado **siempre es coherente con lo que hay en disco**.
Si el `.exe` dice `0.1.0`, el instalador se llama `Convolver-0.1.0-...`. Es
imposible producir un instalador etiquetado 0.2.0 que contenga un binario 0.1.0.

De regalo, el mismo mecanismo unifica `CompanyName` (→ `AppPublisher`) y
`ProductName`. Por eso `app.json` no los repite.

### Cómo sacar una versión nueva

Se edita **una sola línea**, `CMakeLists.txt:3`:

```diff
- project(Convolver VERSION 0.1.0 LANGUAGES C CXX)
+ project(Convolver VERSION 0.2.0 LANGUAGES C CXX)
```

Y después, en este orden:

```powershell
# 1. Recompilar. Esto regenera el recurso de version dentro del .exe.
cmake --build --preset x64-release

# 2. Empaquetar
.\tools\dist\build-installer.ps1
```

Resultado: `dist\Convolver-0.2.0-win64-setup.exe`.

**Lo que NO se toca:** `packaging/app.json`, `packaging/installer.iss`, los
íconos, ni ningún otro archivo. Todo lo demás se acomoda solo, porque el
empaquetador **lee la versión del binario** en vez de declararla.

Lo único que hay que respetar es el orden: si empaquetás sin recompilar, el
instalador sale con la versión **vieja** — y el chequeo de frescura te lo avisa,
porque `CMakeLists.txt` va a quedar más nuevo que el `.exe`.

> ⚠️ Este procedimiento **no funcionaba** hasta que se arregló un defecto de
> JUCE que hacía que el recurso de versión del `.exe` nunca se regenerara.
> El detalle y el arreglo están en §7 — **es obligatorio copiar ese bloque** al
> replicar el flujo en otro repo, o el cambio de versión va a fallar en silencio.

**Recomendado: commitear y taguear antes de empaquetar.** El driver imprime la
revisión de git, y si la versión está sin commitear vas a ver
`+ uncommitted changes`. Eso significa que el instalador quedó atado a un árbol
sucio y no se puede reproducir desde un tag:

```powershell
# editar CMakeLists.txt:3, y despues
git commit -am "Bump version to 0.2.0"
git tag v0.2.0
cmake --build --preset x64-release
.\tools\dist\build-installer.ps1
```

Convención: **`x.y.z`**. JUCE y el `VersionInfoVersion` de Inno la aceptan; el
driver normaliza hasta 4 componentes (`x.y.z.w`) por si algún día hace falta.

### Chequeo de frescura

El driver compara el mtime del `.exe` contra el del **fuente compilado más
reciente** (`Source/**`, `CMakeLists.txt`, `appicon.svg`, `appicon.png`).

- Cubre el caso "cambié la versión y no recompilé" **sin leer CMakeLists.txt**:
  si bumpeás `PROJECT_VERSION` y no recompilás, `CMakeLists.txt` queda más nuevo
  que el `.exe` y avisa.
- **Solo advierte, no aborta.** Los `checkout` de git reescriben mtimes, así que
  habría falsos positivos. `-Force` silencia la advertencia.
- **Excluye `appicon.ico`**: ese asset lo lee Inno, no se compila en el `.exe`,
  así que no puede justificar un aviso.

---

## 5. Procedimiento de release

```powershell
# 1. Compilar y verificar en VS Code (o por consola)
cmake --preset x64                # el configure cambió de nombre: antes x64-release
cmake --build --preset x64-release

# 2. Ver el plan sin compilar nada
.\tools\dist\build-installer.ps1 -CheckOnly

# 3. Empaquetar
.\tools\dist\build-installer.ps1
```

Salida: `dist\Convolver-0.1.0-win64-setup.exe` (~4.5 MB) más el SHA-256 del
`.exe` empaquetado, el del instalador y la revisión de git.

### Checklist de verificación antes de distribuir

- [ ] Se compiló **Release**, nunca Debug (27 MB vs 8.5 MB)
- [ ] El driver no advirtió sobre binario viejo (o entendiste por qué usaste `-Force`)
- [ ] **Instalar en una máquina limpia** y confirmar que `Convolver.exe` en la
      carpeta de instalación **no tiene MOTW**:
      `.\tools\dist\unblock-distribution.ps1 -Path "<carpeta>" -CheckOnly`
- [ ] Aparece en "Aplicaciones instaladas" con la versión correcta
- [ ] Se desinstala limpio
- [ ] **Upgrade: instalar 0.1.0 y después 0.1.1 debe REEMPLAZAR, no duplicar**
      (es la prueba del `AppId`)

### Las 5 superficies del ícono

| # | Superficie | Origen |
|---|---|---|
| 1 | `Convolver.exe` en el Explorador (16/32/48/256) | `ICON_BIG` → PNG |
| 2 | Barra de título de la ventana principal | `appicon.svg` vía `AppIcon.h` |
| 3 | Botón en la barra de tareas | ídem |
| 4 | Ventana de Ayuda (título + taskbar) | ídem |
| 5 | Asistente del instalador + "Aplicaciones instaladas" + accesos directos | `appicon.ico` |

**Nota sobre Windows:** JUCE envía **un solo HICON** para `ICON_BIG` y
`ICON_SMALL` (`juce_Windowing_windows.cpp`), así que el bitmap que devuelve
`createAppIcon()` es el que Windows reduce para la taskbar. De ahí que el SVG
vectorial importe: el ícono de 16 px de la taskbar se dibuja nativamente.

---

## 6. Replicar el flujo en otro repo JUCE

El objetivo de diseño es que **el `.iss` y el driver sean idénticos entre repos**,
y que lo único distinto sea `app.json`. Si al replicar te encontrás editando
`installer.iss` o `build-installer.ps1`, algo se está haciendo de más: avisá,
porque significa que la parametrización tiene un hueco.

### Paso 0 — Prerrequisito

Inno Setup **6.3 o superior** (probado con 7.1.0), instalado **all users**:
`C:\Program Files\Inno Setup 7\ISCC.exe`. El driver lo autodetecta desde el
registro (`Uninstall\Inno Setup*`), así que la ruta **no** se hardcodea en ningún
lado. Si el repo va a buildear en otra máquina, ahí también hace falta.

### Paso 1 — Copiar tal cual, sin editar una línea

| Archivo | Por qué es genérico |
|---|---|
| `tools/dist/build-installer.ps1` | Saca todo de `app.json` + del recurso de versión del `.exe` |
| `packaging/installer.iss` | Recibe todo por `/D`; tiene guardas `#error` si falta un define |
| `tools/icons/make-app-icon.ps1` | Geometría propia y aislada; ver Paso 5 |

### Paso 2 — `packaging/app.json`

Generá un **GUID nuevo** — `(New-Guid).Guid.ToUpper()` en PowerShell — y ponelo
**sin llaves**:

```jsonc
{
  "appId": "GENERA-UN-GUID-NUEVO-ACA",
  "appName": "NombreDeLaApp",          // como aparece en Inicio y en Aplicaciones instaladas
  "exeName": "NombreDeLaApp.exe",
  "artefactDir": "build/NombreDeLaApp_artefacts/Release",
  "iconFile": "packaging/icon/appicon.ico",
  "runtimeLogName": "nombredelaapp_runtime.log",
  "licenseFile": null                   // o una ruta relativa al repo
}
```

**No pongas la versión ni el publisher acá.** Salen del `.exe`.

### Paso 3 — `CMakeLists.txt`

Cuatro cambios. El bloque de versión y copyright:

```cmake
project(NombreDeLaApp VERSION 0.1.0 LANGUAGES C CXX)   # ← UNICA fuente de la version

juce_add_gui_app(NombreDeLaApp
    PRODUCT_NAME       "NombreDeLaApp"
    COMPANY_NAME       "TuNombre"                       # ← se vuelve AppPublisher
    COMPANY_COPYRIGHT  "(C) 2026 TuNombre"
    VERSION            "${PROJECT_VERSION}"             # ← no hardcodear
    ICON_BIG           "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon/appicon.png"
    ...)
```

El runtime estático **en los dos targets** (si falta en el segundo, `LNK4098`):

```cmake
if(MSVC)
    set(APP_MSVC_RUNTIME "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET NombreDeLaApp PROPERTY MSVC_RUNTIME_LIBRARY "${APP_MSVC_RUNTIME}")
endif()

juce_add_binary_data(NombreDeLaAppAssets
    HEADER_NAME "NombreDeLaAppAssets.h"
    NAMESPACE   "NombreDeLaAppAssets"
    SOURCES     packaging/icon/appicon.svg)

if(MSVC)
    set_property(TARGET NombreDeLaAppAssets PROPERTY MSVC_RUNTIME_LIBRARY "${APP_MSVC_RUNTIME}")
endif()
```

Y `NombreDeLaAppAssets` en el `target_link_libraries`.

**Este cuarto cambio es el que más fácil se olvida, y sin él el cambio de versión
falla en silencio.** Va inmediatamente después de `juce_add_gui_app`, con el
nombre del target reemplazado:

```cmake
# El .rc generado por JUCE depende del icono pero NO de Info.txt, que es donde
# vive la version. Sin esto, cambiar PROJECT_VERSION no regenera el .rc, el .exe
# se relinkea contra el recurso viejo y sigue reportando la version anterior,
# sin ningun error que lo explique. Ver §7.
file(GLOB _app_info_file "${CMAKE_CURRENT_BINARY_DIR}/NombreDeLaApp_artefacts/JuceLibraryCode/Info.txt")
file(GLOB _app_rc_files  "${CMAKE_CURRENT_BINARY_DIR}/NombreDeLaApp_artefacts/JuceLibraryCode/*_resources.rc")

foreach(_info_file IN LISTS _app_info_file)
    foreach(_rc_file IN LISTS _app_rc_files)
        if(EXISTS "${_rc_file}" AND "${_info_file}" IS_NEWER_THAN "${_rc_file}")
            file(REMOVE "${_rc_file}")
            message(STATUS "Version resource is stale - regenerating ${_rc_file}")
        endif()
    endforeach()
endforeach()
```

**Verificación de que quedó bien:** cambiá `PROJECT_VERSION` a otro valor,
recompilá, y confirmá que el `.exe` lo reporta:

```powershell
(Get-Item "build\NombreDeLaApp_artefacts\Release\NombreDeLaApp.exe").VersionInfo.ProductVersion
```

### Paso 4 — `Source/AppIcon.h`

Copiarlo y cambiar **solo** el namespace del header de binary data:

```cpp
#include <NombreDeLaAppAssets.h>
...
    static const std::unique_ptr<juce::Drawable> artwork =
        juce::Drawable::createFromImageData (NombreDeLaAppAssets::appicon_svg,
                                             (size_t) NombreDeLaAppAssets::appicon_svgSize);
```

El nombre del símbolo sale del nombre del archivo: `appicon.svg` → `appicon_svg`.

### Paso 5 — Los assets de ícono

**Primero verificá la convención de artefactos del repo destino.** No la adivines:
Convolver y LogSweepGenerator usan `build/`, y **ADM además no tiene
`CMakePresets.json` ni build Release** (solo Debug). Eso define `artefactDir`.

Después corré `.\tools\icons\make-app-icon.ps1` y reemplazá el diseño si querés
otro arte. Los tres archivos se generan juntos desde la misma geometría, así que
no pueden divergir.

### Paso 6 — `.gitignore` y `.vscode`

- Agregar `dist/` (salida del empaquetado).
- Copiar `.vscode/settings.json` ajustando `cmake.configurePreset` /
  `cmake.buildPreset` a los nombres reales del repo destino.

### Paso 7 — Verificar end-to-end

```powershell
cmake --preset x64
cmake --build --preset x64-release
.\tools\dist\build-installer.ps1 -CheckOnly    # ver el plan, sin compilar
.\tools\dist\build-installer.ps1
```

Y después el checklist de §5. **El mínimo indispensable:** instalar en una máquina
limpia y confirmar que el `.exe` instalado no tiene MOTW.

### Lo que NO hay que hacer

- No agregar un paso de compilación al driver. Su valor es empaquetar **exactamente**
  el binario que ya verificaste.
- No declarar la versión en `app.json` ni en el `.iss`.
- No cambiar el `appId` al sacar una versión nueva: rompe el upgrade in-place.
- No editar `installer.iss` para un caso particular sin parametrizarlo por `/D`.

---

## 7. Errores ya cometidos, para no repetirlos

| Síntoma | Causa | Solución |
|---|---|---|
| `LNK4098: defaultlib 'MSVCRTD' conflicts` | `juce_add_binary_data` crea una librería **independiente** que no hereda el `MSVC_RUNTIME_LIBRARY` del ejecutable → `/MDd` contra `/MTd` | Aplicar el runtime también al target de binary data. No es solo ruido: un binario con dos CRTs tiene comportamiento indefinido |
| **Cambiás la versión, recompilás, y el `.exe` sigue reportando la versión vieja** (sin ningún error) | El `add_custom_command` que genera `Convolver_resources.rc` en [`_juce_add_resources_rc`](file:///C:/JUCE/extras/Build/CMake/JUCEUtils.cmake) declara **solo el ícono** como `DEPENDS`, no `Info.txt`, que es donde vive la versión. El `.rc` nunca se regenera y el `.exe` se relinkea contra el recurso viejo | Ya resuelto en `CMakeLists.txt`: al configurar, se borra el `.rc` si `Info.txt` es más nuevo, forzando la regeneración. **Si replicás el flujo en otro repo, no te olvides de copiar ese bloque** |
| `Error: Unknown constant "99C8..."` | Inno interpreta `{GUID}` como referencia a constante | `AppId` sin llaves en `app.json`, `AppId={{{#AppId}}` en el `.iss` |
| `Resource update error: Icon file is invalid` | `SetupIconFile` **solo** acepta `.ico`, no PNG ni SVG, y **no** existe la sintaxis `,index` para esta directiva | Generar `appicon.ico` real |
| `.ico` generado vacío o sin 48/256 | `ICON_BIG` apunta a un SVG → `getWidth()` = 0 | Usar PNG ≥256 como `ICON_BIG` |
| Íconos de ventana/taskbar borrosos | Reducir el PNG de 512 al tamaño final en un solo paso | Rasterizar el SVG al tamaño pedido |
| Log de runtime nunca se escribe en la app instalada | `Main.cpp` escribía junto al `.exe`, que es de solo lectura bajo `Program Files` | Fallback a `%APPDATA%\Convolver\` |
| `Setup was unable to create the directory "...\Temp\is-XXXX.tmp". Error 5: Access is denied.` | Lanzar el instalador **desde dentro del workspace del agente**. `Setup.exe` de Inno es un stub: tiene que extraer el motor a `%TEMP%` antes de poder correr, y el sandbox del harness corre los procesos en **Low Integrity**, que no puede escribir en un `%TEMP%` Medium IL | Copiar el instalador fuera del repo (Escritorio, `Downloads`) y ejecutarlo desde ahí. **No afecta a usuarios reales**: descargan a `Downloads` y lo corren fuera de cualquier sandbox |
| Después de "Delete Cache and Reconfigure", IntelliSense marca `cannot open source file "JuceHeader.h"` en todos los headers | `JuceHeader.h` se genera en **tiempo de build** (paso `CustomBuild` de juceaide), no al configurar | Compilar una vez. No es un problema de CMake, no hay nada que arreglar |
| El driver avisa "executable looks older than the source" sin haber cambiado código | Un `git checkout` reescribió los mtimes, o se regeneró un asset de ícono | `-Force`. El generador de íconos es idempotente: no reescribe un asset cuyo contenido no cambió, así que no lo causa |

---

## 8. Licencia de Inno Setup

Inno Setup **solicita** una licencia comercial, pero no la exige:

- **Non-commercial: no se pide.**
- For-profit con facturación **> USD 5000/año**: se pide (Single User / Team / Enterprise).
- Solo uso interno, sin distribuir: también se pide.
- **Si todavía no publicaste instaladores en producción: no corresponde comprarla.**

El compilador imprime `Non-commercial use only` en cada corrida mientras no haya
una clave instalada. Los instaladores generados **no quedan marcados** como sin
licencia. Alternativas 100 % libres si se quiere evitar la pregunta: **NSIS** o
**WiX + CPack**.

Fuente: <https://jrsoftware.org/isorder.php>

---

## 9. Referencias

- [Inno Setup — descargas](https://jrsoftware.org/isdl.php) (probado con 7.1.0, instalado all-users)
- [SetupIconFile](https://jrsoftware.org/ishelp/topic_setup_setupiconfile.htm) — confirma que exige `.ico`
- [Architecture Identifiers](https://jrsoftware.org/is6help/topic_archidentifiers.htm) — `x64compatible` vs `x64os`; `x64` quedó deprecado en 6.3
- [PrivilegesRequired](https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm) — `lowest` = non administrative install mode
- [Constants](https://jrsoftware.org/ishelp/topic_consts.htm) — tabla de auto constantes: `{autopf}` → `{userpf}` sin privilegios
- [Licencias comerciales](https://jrsoftware.org/isorder.php)
- `windows_trust_and_distribution.md` — SmartScreen, MOTW, y por qué el instalador es la respuesta gratuita
