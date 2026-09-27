# Empaquetar una app JUCE en Windows con Inno Setup

**Estado:** flujo verificado end-to-end (2026-09). Referencia: Convolver 0.2.0 —
instalación completa, `.exe` instalado sin MOTW, y upgrade in-place probados.

**Para quién es esto:** un agente que tenga que replicar el mismo flujo de
empaquetado en **otro repo JUCE**. Seguí los pasos en orden. Los bloques de código
están listos para copiar, sustituyendo los nombres del repo destino.

---

## Objetivo

Al terminar tiene que existir:

- `tools/dist/build-installer.ps1` — un comando que produce el instalador
- `packaging/app.json`, `packaging/installer.iss`, `packaging/icon/`
- `dist/<App>-<version>-win64-setup.exe`

Tres propiedades no negociables:

1. **La versión se declara una sola vez**, en `CMakeLists.txt`. El empaquetador la
   lee del `.exe`; no la vuelve a declarar.
2. **El empaquetador no compila.** Empaqueta el Release que ya está en disco, para
   que sea exactamente el binario que se verificó.
3. **Un solo asset de arte** alimenta el ícono del `.exe` y los de las ventanas.

Contexto de negocio (por qué un instalador y no un ZIP) está en
`windows_trust_and_distribution.md` §B. Resumen: el instalador **extrae** el
`.exe` como archivo nuevo, y un archivo nuevo no hereda el Mark-of-the-Web. El
usuario ve los avisos **una sola vez**, al instalar, y la app arranca limpia.

---

## Paso 0 — Prerrequisito

Inno Setup **6.3 o superior**, instalado **all users**. Probado con 7.1.0.

```powershell
# Verificar que esta y donde
Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup*_is1' |
    Select-Object DisplayName, InstallLocation
# Esperado: C:\Program Files\Inno Setup 7\
```

El driver autodetecta `ISCC.exe` desde el registro (y si no, desde las rutas
típicas y el PATH), así que **la ruta no se hardcodea en ningún archivo**.

---

## Paso 1 — Averiguar los datos del repo destino

**No adivinar ninguno.** Completar esta tabla antes de tocar código:

| Dato | Cómo obtenerlo | Ejemplo (Convolver) |
|---|---|---|
| Nombre del producto | `PRODUCT_NAME` en `juce_add_gui_app` | `Convolver` |
| Nombre del `.exe` | buildear y mirar | `Convolver.exe` |
| **Carpeta de artefactos Release** | buildear y buscar el `.exe` | `build/Convolver_artefacts/Release` |
| Nombre del log de runtime | `FileLogger` en `Source/Main.cpp` | `convolver_runtime.log` |
| Target de `juce_add_gui_app` | `CMakeLists.txt` | `Convolver` |
| ¿Tiene `CMakePresets.json`? | | sí |

⚠️ **La carpeta de artefactos no es uniforme entre repos.** No asumir `build/`.
En los repos conocidos, unos usan `build/` y otro no tiene presets ni build
Release. Si no hay build Release, ese es un problema previo que hay que resolver
antes de continuar.

---

## Paso 2 — Copiar tres archivos sin editar

| Copiar de este repo | Destino | Por qué es genérico |
|---|---|---|
| `tools/dist/build-installer.ps1` | igual | Saca todo de `app.json` + del `.exe` |
| `packaging/installer.iss` | igual | Recibe todo por `/D`; tiene guardas `#error` |
| `tools/icons/make-app-icon.ps1` | igual | Geometría propia y aislada |

**Si tuviste que editar alguno de los tres, la parametrización tiene un hueco.**
Reportalo en vez de hardcodear: la premisa de diseño es que estos archivos sean
idénticos entre repos.

---

## Paso 3 — `packaging/app.json`

Generá un **GUID nuevo**: `(New-Guid).Guid.ToUpper()`. Va **sin llaves**.

```jsonc
{
  "appId": "GENERA-UN-GUID-NUEVO-ACA",
  "appName": "NombreDeLaApp",
  "exeName": "NombreDeLaApp.exe",
  "artefactDir": "build/NombreDeLaApp_artefacts/Release",
  "iconFile": "packaging/icon/appicon.ico",
  "runtimeLogName": "nombredelaapp_runtime.log",
  "licenseFile": null
}
```

| Campo | Notas |
|---|---|
| `appId` | **Único por app e inmutable para siempre.** Si cambia entre versiones, cada upgrade se instala **al lado** de la anterior en vez de reemplazarla |
| `artefactDir` | Relativo a la raíz del repo. Del Paso 1 |
| `iconFile` | El `.ico` que genera el script de íconos |
| `licenseFile` | `null`, o una ruta relativa al repo |

**No agregar versión, publisher ni nombre de producto.** Se leen del `.exe`.

---

## Paso 4 — `CMakeLists.txt`

Cinco cambios. Los nombres son de Convolver; sustituir por los del destino.

### 4.1 — La versión, una sola vez

```cmake
project(NombreDeLaApp VERSION 0.1.0 LANGUAGES C CXX)
                        ^^^^^^ la unica declaracion de la version
```

### 4.2 — `juce_add_gui_app`

```cmake
juce_add_gui_app(NombreDeLaApp
    PRODUCT_NAME       "NombreDeLaApp"
    COMPANY_NAME       "TuNombre"                        # se vuelve AppPublisher
    COMPANY_COPYRIGHT  "(C) 2026 TuNombre"               # llena LegalCopyright
    VERSION            "${PROJECT_VERSION}"              # NO hardcodear
    ICON_BIG           "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon/appicon.png"
    ...)
```

### 4.3 — Regenerar el recurso de versión ⚠️ **obligatorio**

Sin este bloque, cambiar la versión **falla en silencio**: el `.exe` sigue
reportando la versión anterior y no aparece ningún error. Va justo después de
`juce_add_gui_app`.

> Causa: en `_juce_add_resources_rc` de JUCE, el `add_custom_command` que genera
> `*_resources.rc` declara **solo el ícono** en `DEPENDS`, no `Info.txt`, que es
> donde vive la versión. Sin dependencia, el `.rc` nunca se regenera.

```cmake
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

### 4.4 — Runtime MSVC estático en los **dos** targets ⚠️

Si se aplica solo al ejecutable, el linker mezcla dos CRTs (`LNK4098`), que es
comportamiento indefinido, no un warning cosmético.

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

### 4.5 — Linkear el target de assets

```cmake
target_link_libraries(NombreDeLaApp PRIVATE
    NombreDeLaAppAssets
    ...)
```

---

## Paso 5 — `Source/AppIcon.h`

Copiar el de este repo y cambiar **solo el namespace** del header de binary data.
El símbolo sale del nombre del archivo: `appicon.svg` → `appicon_svg`.

```cpp
#include <NombreDeLaAppAssets.h>
...
    static const std::unique_ptr<juce::Drawable> artwork =
        juce::Drawable::createFromImageData (NombreDeLaAppAssets::appicon_svg,
                                             (size_t) NombreDeLaAppAssets::appicon_svgSize);
```

Después, **conectar el ícono en las ventanas** del repo destino. Buscar dónde la
app setea el ícono y usar `createAppIcon()`:

```cpp
const auto appIcon = createAppIcon();     // default 32 px
setIcon (appIcon);
if (auto* peer = getPeer())
    peer->setIcon (appIcon);
```

Si el repo no tiene `AppIcon.h`, hay que crearlo y wirearlo en **todas** las
ventanas (principal y diálogos).

---

## Paso 6 — Generar los assets de ícono

```powershell
.\tools\icons\make-app-icon.ps1
```

Escribe tres archivos, todos **commiteados** (son entrada del build):

| Asset | Lo consume | Formato |
|---|---|---|
| `appicon.svg` | `juce_add_binary_data` → íconos de ventana/taskbar/diálogo | Vector |
| `appicon.png` (512²) | `juce_add_gui_app(ICON_BIG)` → ícono del `.exe` | Raster |
| `appicon.ico` | `SetupIconFile` → ícono del instalador | ICO multi-resolución |

**Los tres formatos son necesarios, no es redundancia:**

- `ICON_BIG` **no acepta SVG**. juceaide lo resuelve a un `DrawableComposite`, que
  nunca llama a `Component::setBounds()`, así que `getWidth()` da 0, todos los
  tamaños se descartan y el `.ico` sale **vacío**. Necesita un raster de **≥256 px**.
- El **runtime** quiere vector: rasteriza al tamaño exacto, y así el ícono de 16 px
  de la taskbar queda nítido en vez de ser un bitmap reducido.
- `SetupIconFile` **solo acepta `.ico`**, y no existe la sintaxis `,index` para esa
  directiva. El `.ico` generado incluye 64², que Inno recomienda y juceaide no emite.

El script es **idempotente**: no reescribe un asset cuyo contenido no cambió, para
no mover timestamps y disparar en falso el chequeo de frescura.

Ajustar el diseño (colores, geometría) editando el script, no los assets.

---

## Paso 7 — `.gitignore` y `.vscode`

En `.gitignore`, **anclar la regla con barra inicial**:

```gitignore
/dist/
```

Sin la barra, `dist/` también matchea `tools/dist/` y **excluye en silencio el
driver de empaquetado**. Es un error que ya se cometió una vez.

En `.vscode/settings.json`, ajustar los nombres de preset a los del repo destino:

```jsonc
{
  "cmake.useCMakePresets": "always",
  "cmake.configurePreset": "NOMBRE_DEL_CONFIGURE_PRESET",
  "cmake.buildPreset": "NOMBRE_DEL_BUILD_PRESET_RELEASE",
  "cmake.configureOnOpen": true,
  "files.watcherExclude": { "**/build/**": true, "**/dist/**": true },
  "search.exclude":       { "**/build/**": true, "**/dist/**": true }
}
```

Fijar los presets evita que CMake Tools quede apuntando a un preset inexistente:
cuando eso pasa se queda **sin configuración** y el C/C++ marca
`cannot open source file "JuceHeader.h"` en todos los headers.

---

## Paso 8 — Verificar

```powershell
cmake --preset <preset>
cmake --build --preset <preset-release>

.\tools\dist\build-installer.ps1 -CheckOnly   # muestra el plan sin compilar
.\tools\dist\build-installer.ps1
```

### Definición de terminado

Ninguno de estos es opcional:

- [ ] `dist\<App>-<version>-win64-setup.exe` existe
- [ ] El nombre del instalador lleva la **versión correcta**
- [ ] El `.ico` del `.exe` tiene **16, 32, 48 y 256**:
      ```powershell
      # listar las entradas del .ico generado
      $b=[IO.File]::ReadAllBytes("build\<App>_artefacts\JuceLibraryCode\icon.ico")
      $n=[BitConverter]::ToUInt16($b,4)
      0..($n-1) | ForEach-Object { $w=$b[6+$_*16]; if($w -eq 0){256}else{$w} }
      ```
- [ ] **Instalar en una máquina limpia y confirmar que el `.exe` instalado NO tiene
      MOTW** — es la premisa central de todo esto:
      ```powershell
      $exe = "$env:LOCALAPPDATA\Programs\<App>\<App>.exe"
      Get-Item $exe -Stream *          # debe aparecer SOLO :$DATA
      ```
- [ ] La app instalada **arranca sin ningún cartel**
- [ ] Aparece en "Aplicaciones instaladas" con la versión correcta
- [ ] Se desinstala limpio
- [ ] **Upgrade in-place:** instalar la versión A y después la B. Debe
      **reemplazar**, no duplicar la entrada. Es la prueba del `AppId`

### Las 5 superficies del ícono

Revisar las cinco; es donde más fácil se escapa un problema:

| # | Superficie | Origen |
|---|---|---|
| 1 | `<App>.exe` en el Explorador (probar **iconos extra grandes** → 256) | `ICON_BIG` ← PNG |
| 2 | Barra de título de la ventana principal | `appicon.svg` |
| 3 | Botón en la barra de tareas | `appicon.svg` |
| 4 | Diálogos (título + taskbar) | `appicon.svg` |
| 5 | Asistente del instalador + "Aplicaciones instaladas" + accesos directos | `appicon.ico` |

---

## Mantenimiento: sacar una versión nueva

**Una línea**, `CMakeLists.txt`:

```diff
- project(NombreDeLaApp VERSION 0.1.0 LANGUAGES C CXX)
+ project(NombreDeLaApp VERSION 0.2.0 LANGUAGES C CXX)
```

Y después, **en este orden**:

```powershell
cmake --build --preset <preset-release>
.\tools\dist\build-installer.ps1
```

No se toca nada más: `app.json`, `installer.iss` y los íconos quedan iguales.

**Commitear y taguear antes de empaquetar**, o el driver va a reportar
`+ uncommitted changes` y el instalador quedará atado a un árbol no reproducible:

```powershell
git commit -am "Bump version to 0.2.0"
git tag v0.2.0
cmake --build --preset <preset-release>
.\tools\dist\build-installer.ps1
```

Convención `x.y.z`. El driver normaliza hasta 4 componentes.

**Si empaquetás sin recompilar**, el instalador sale con la versión vieja. El
driver avisa (chequeo de frescura contra los fuentes compilados); `-Force` lo
silencia cuando el aviso es un falso positivo por mtime de git.

---

## Errores conocidos

| Síntoma | Causa | Solución |
|---|---|---|
| Cambiás la versión, recompilás y el `.exe` sigue con la vieja, **sin error** | El `.rc` de JUCE depende del ícono pero no de `Info.txt` | El bloque del Paso 4.3. Es obligatorio |
| `LNK4098: defaultlib 'MSVCRTD' conflicts` | `juce_add_binary_data` crea un target aparte que no hereda el runtime | Paso 4.4: aplicar el runtime a los dos targets |
| `Error: Unknown constant "99C8..."` al compilar el `.iss` | Inno lee `{GUID}` como constante | `appId` sin llaves en `app.json`; `AppId={{{#AppId}}` en el `.iss` |
| `Resource update error: Icon file is invalid` | `SetupIconFile` solo acepta `.ico` | Generar `appicon.ico` (Paso 6) |
| El `.ico` del `.exe` sale vacío o sin 48/256 | `ICON_BIG` apunta a un SVG | Usar PNG ≥256 como `ICON_BIG` |
| Íconos de ventana/taskbar borrosos | Se reduce un PNG de 512 al tamaño final en un paso | Rasterizar el SVG al tamaño pedido |
| `cannot open source file "JuceHeader.h"` en todos los headers | `JuceHeader.h` se genera en **tiempo de build**, no al configurar. Pasa después de "Delete Cache and Reconfigure" | Compilar una vez. **No es un problema de CMake** |
| `Setup was unable to create the directory "...\Temp\is-XXXX.tmp". Error 5: Access is denied.` | Ejecutar el instalador **desde dentro del workspace del agente**. El sandbox del harness corre los procesos en Low Integrity, y Low IL no puede escribir en un `%TEMP%` Medium IL | Copiar el instalador fuera del repo (Escritorio, `Downloads`) y ejecutarlo desde ahí. **No afecta a usuarios reales** |
| El driver avisa "executable looks older than the source" sin cambios de código | Un `git checkout` reescribió los mtimes | `-Force` |
| `tools/dist/*.ps1` no aparece en `git status` | Regla `dist/` sin anclar en `.gitignore` | `/dist/` (Paso 7) |

---

## Archivos y responsabilidades

| Archivo | Rol |
|---|---|
| `packaging/app.json` | **Lo único específico de la app** |
| `packaging/installer.iss` | Plantilla Inno agnóstica; todo llega por `/D` |
| `tools/dist/build-installer.ps1` | Driver: valida, absorbe la versión del `.exe`, invoca ISCC |
| `tools/icons/make-app-icon.ps1` | Genera los 3 assets desde una geometría |
| `packaging/icon/appicon.{svg,png,ico}` | Assets generados, commiteados |
| `dist/` | Salida del empaquetado. En `.gitignore` |

### Ajustes del instalador que ya trae la plantilla

| Directiva | Valor | Por qué |
|---|---|---|
| `PrivilegesRequired` | `lowest` | Instalación per-user en `%LOCALAPPDATA%\Programs` → **sin UAC** |
| `ArchitecturesAllowed` | `x64compatible` | `x64` quedó deprecado en Inno 6.3 y significa `x64os`, que excluye Arm64 con emulación |
| `WizardStyle` | `modern` | |
| `Compression` | `lzma2/max` | |
| `[Languages]` | english + spanish | Cambiar si el destino no necesita español |

**No editar `installer.iss` para un caso particular sin parametrizarlo por `/D`.**

---

## Licencia de Inno Setup

Inno Setup **solicita** una licencia comercial pero no la exige: no se pide para
uso non-commercial, ni mientras no se hayan publicado instaladores en producción.
El compilador imprime `Non-commercial use only` en cada corrida si no hay clave.
Los instaladores generados no quedan marcados.

Alternativas 100 % libres si se quiere evitar la pregunta: NSIS, o WiX con CPack.
Fuente: <https://jrsoftware.org/isorder.php>
