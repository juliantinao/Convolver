# Empaquetar una app JUCE en Windows con Inno Setup

**Estado:** flujo verificado end-to-end (2026-09) en **dos** repos:

- **Convolver 0.2.0** — instalación completa, `.exe` instalado sin MOTW, y upgrade
  in-place probados.
- **LogSweepGenerator 0.1.1** — versión única, ícono multi-resolución, runtime
  estático verificado con `dumpbin`, y `LNK4098` ausente.

Las dos pasadas encontraron huecos **distintos**. Los dos están incorporados acá:
donde diga "en Convolver pasó X" o "en LogSweepGenerator pasó Y", es porque ese
repo ya pagó ese error. No lo repitas.

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
`windows_trust_and_distribution.md` §B — un documento que hoy vive **solo en el
repo de Convolver**: si el destino va a necesitar el resto del material sobre
SmartScreen, MOTW y firma de código, copialo también. Resumen: el instalador
**extrae** el `.exe` como archivo nuevo, y un archivo nuevo no hereda el
Mark-of-the-Web. El usuario ve los avisos **una sola vez**, al instalar, y la app
arranca limpia.

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
| Nombre del log de runtime | `FileLogger` en `Main.cpp`, o el nombre que tendría (Paso 3) | `convolver_runtime.log` |
| Target de `juce_add_gui_app` | `CMakeLists.txt` | `Convolver` |
| ¿Tiene `CMakePresets.json`? | | sí |
| Nombres de los presets | `CMakePresets.json` | `default` / `release` |
| **Carpeta de fuentes** | mirar la raíz del repo | `Source/` (Convolver), `src/` (LogSweepGenerator) |

⚠️ **La carpeta de artefactos no es uniforme entre repos.** No asumir `build/`.
En los repos conocidos, unos usan `build/` y otro no tiene presets ni build
Release. Si no hay build Release, ese es un problema previo que hay que resolver
antes de continuar.

⚠️ **El aviso de frescura solo mira `Source/` y `CMakeLists.txt`.** Lo hace con
`Test-Path`, así que en Windows `Source` matchea `src` por ser case-insensitive
— pero **no** matchea `app/`, `code/` ni ninguna otra carpeta. Si las fuentes
viven en otro lado, el chequeo **no salta nunca y no avisa**: podés empaquetar un
binario viejo sin que nadie te lo diga. Verificá que la carpeta del Paso 1 exista,
o ajustá la ruta en el driver (ver "Errores conocidos").

---

## Paso 2 — Qué copiar tal cual y qué adaptar

La premisa es que la **lógica** sea idéntica entre repos. Los literales de
presentación y la geometría del arte **no** pueden serlo: son del repo destino.

### 2.1 — Copiar y tocar **una sola línea**

| Copiar de este repo | Destino | Por qué es agnóstico |
|---|---|---|
| `packaging/installer.iss` | igual | Todos los valores llegan por `/D`; tiene guardas `#error` |

La única línea app-específica es el comentario de cabecera
(`;  Convolver installer script.`). Cambialo por el nombre de tu app. Es un
comentario, no afecta el instalador, pero dejarlo con el nombre de otra app
confunde a quien lo lea después.

⚠️ **No intentes parametrizarlo con `{#AppName}`.** ISPP **no** expande defines
dentro de comentarios `;`: verificado con una prueba — un `{#Define}` inexistente
en un comentario `;` compila sin error, o sea que la sustitución no ocurre. Es la
excepción a "todo llega por `/D`".

### 2.2 — Copiar y adaptar literales

| Archivo | Qué cambiar |
|---|---|
| `tools/dist/build-installer.ps1` | El `.SYNOPSIS` (nombre de la app), el banner `=== <App> :: build installer ===`, el hint `cmake --preset <...>` de cuando falta el `.exe`, y el mensaje final de MOTW |
| `tools/icons/make-app-icon.ps1` | El `.SYNOPSIS`, el texto de `ICON_BIG` en el `.DESCRIPTION`, la línea de reporte del `.png`, y **la geometría** (colores y forma, en el bloque `design`) |

Los dos están escritos para Convolver: sin adaptarlos, el empaquetador de tu app
imprime `=== Convolver :: build installer ===` y sugiere presets que no existen en
tu repo. **Eso no es un hueco que haya que reportar, es trabajo del Paso 2.**

⚠️ **El mensaje final de `build-installer.ps1` cierra sugiriendo
`tools/dist/unblock-distribution.ps1`**, que este paso no copia. O copiás ese
script también (también tiene literales de Convolver), o reemplazás el hint por el
comando de verificación de MOTW de la Definición de terminado.

### 2.3 — Lo que sí es un hueco

Solo si tuviste que tocar la **lógica**. La premisa de diseño es que los nombres,
la versión, las rutas y los presets salgan de `app.json` + del `.exe` + del
`CMakePresets.json`. Si tuviste que hardcodear algo para que funcionara, reportalo
en vez de dejarlo: es exactamente lo que va a romper el próximo repo.

### 2.4 — Opcionales, no parte del flujo

| Archivo | Para qué |
|---|---|
| `tools/dist/unblock-distribution.ps1` | Limpia el MOTW de una carpeta y reporta el estado de SmartScreen. Sirve para diagnosticar, no para empaquetar |
| `tools/dist/inspect-signature.ps1` | Reporta el estado Authenticode y las firmas de cada payload |

Ninguno hace falta para producir el instalador, y los dos tienen rutas y nombres
de Convolver hardcodeados.

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
| `runtimeLogName` | Nombre del log que la app escribe junto a su `.exe`. Lo usa `[UninstallDelete]` para no dejar basura |
| `licenseFile` | `null`, o una ruta relativa al repo |

**No agregar versión, publisher ni nombre de producto.** Se leen del `.exe`.

⚠️ **`runtimeLogName` es obligatorio aunque la app no loguee nada.** La plantilla
tiene `#ifndef RuntimeLogName #error`, así que `installer.iss` no compila sin ese
define. Si el repo no tiene `FileLogger` en `Main.cpp` — LogSweepGenerator no lo
tenía — declaralo igual con el nombre que tendría: la entrada `[UninstallDelete]`
sobre un archivo que nunca existió es inocua, y el día que agregues logging no hay
que volver a tocar el instalador.

---

## Paso 4 — `CMakeLists.txt`

Cinco cambios. Los nombres son de Convolver; sustituir por los del destino.

### 4.1 — La versión, una sola vez

```cmake
project(NombreDeLaApp VERSION 0.1.0 LANGUAGES C CXX)
                        ^^^^^^ la unica declaracion de la version
```

"Una sola vez" no se cumple solo por escribirlo acá: hay que **sacar las copias
que ya existan**. La que más se escapa es la que la app se reporta a sí misma:

```cpp
// Main.cpp — ANTES: una segunda declaración, que se desincroniza en silencio
const juce::String getApplicationVersion() override { return "1.1.0"; }

// DESPUÉS: lee el mismo recurso del que el instalador absorbe la versión
const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
```

En LogSweepGenerator el `.exe` decía `0.1.0`, la app decía `1.1.0`, y nada avisaba
de la diferencia. `ProjectInfo::projectName` resuelve lo mismo para el nombre.
Antes de seguir, buscá hardcodeos: `git grep -n "1\.1\.0"`.

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

### 4.4 — Runtime MSVC estático ⚠️

El `.exe` **no debe depender** de `msvcp140.dll` / `vcruntime140.dll` /
`concrt140.dll`. Si dependiera, el instalador tendría que arrastrar el VC++
Redistributable (~25 MB y un reinicio potencial) para no fallar en una máquina
limpia. Con el CRT estático el instalador queda **sin prerrequisitos**.

Hay dos formas de fijarlo. **Preferí la global** salvo que el repo ya use la otra.

**Opción A — variable global, antes de cualquier target (preferida):**

```cmake
# Va ARRIBA de todo: antes de juce_add_gui_app y de add_subdirectory(JUCE).
#
# Una variable de directorio se copia a la propiedad del target en el momento en
# que el target se crea, así que todo target definido después la hereda —
# incluido el de juce_add_binary_data.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

**Opción B — target por target:**

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

Por qué la diferencia importa:

- **B es frágil.** Cubre exactamente los targets que enumeraste. El día que
  aparezca otro target propio — un binary data nuevo, una lib de tests — queda en
  `/MD` por default, el linker mezcla dos CRTs y salta `LNK4098`. Eso es
  literalmente lo que pasó en Convolver: `03290ec` linkeó estático solo el
  ejecutable, y el `juce_add_binary_data` que agregó `ac6bdb5` después reintrodujo
  el problema.
- **A es inmune a eso**, porque no enumera targets. Es la que usa
  LogSweepGenerator y su build Release no reporta `LNK4098`.
- **No mezcles las dos sin motivo.** Si el repo ya tiene B y funciona, dejalo; el
  punto es no volver a agregar un target sin runtime.

`LNK4098` no es un warning cosmético: un binario con dos CRTs linkeados tiene
comportamiento indefinido.

### 4.5 — Linkear el target de assets

```cmake
target_link_libraries(NombreDeLaApp PRIVATE
    NombreDeLaAppAssets
    ...)
```

---

## Paso 5 — `<carpeta-de-fuentes>/AppIcon.h`

Copiar el de este repo y cambiar **solo el namespace** del header de binary data.
La carpeta es la del Paso 1: `Source/` en Convolver, `src/` en LogSweepGenerator.
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

⚠️ **Revisá las instrucciones para agentes del repo destino**
(`.github/copilot-instructions.md`, `AGENTS.md`) antes de tocarlo. Es un archivo
que este flujo **sí** modifica, y en LogSweepGenerator figuraba en la lista de "no
modificar": hubo que actualizar esa instrucción, porque el ícono dejó de dibujarse
a mano en el header y pasó a rasterizar el SVG embebido. Si la instrucción y este
Paso se contradicen, gana este Paso — pero dejá la instrucción alineada o el
próximo agente va a revertir el cambio.

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

⚠️ **Borrá cualquier `cmake.generator` que ya estuviera en el archivo.** Con
`useCMakePresets: always` esa clave pelea contra el preset, y si apunta a una
versión de Visual Studio que no está instalada el configure falla. En
LogSweepGenerator el `settings.json` decía `"Visual Studio 17 2022"` mientras el
preset y el VS instalado eran 18 2026.

Lo mismo vale para `.vscode/tasks.json`: si el task `configure` hardcodea
`cmake -S . -B build -G "<generador>"`, cambialo por `cmake --preset <preset>`. Un
generador que ya no está instalado rompe el task sin explicar por qué.

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
- [ ] El build Release **no imprime `LNK4098`**
- [ ] El `.exe` **no importa DLLs del runtime MSVC**:
      ```powershell
      dumpbin /nologo /dependents build\<App>_artefacts\Release\<App>.exe |
          Select-String "msvcp|vcruntime|concrt"
      # Sin resultados. Si aparece alguna, el runtime estático no se aplicó, o se
      # aplicó después de crear el target: ver Paso 4.4.
      ```
      Lo esperado es un puñado de DLLs del OS (`KERNEL32`, `USER32`, `GDI32`,
      `d2d1`, `d3d11`, `dcomp`, `dwmapi`, …) y **nada más**
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
| **Versión** | | |
| Cambiás la versión, recompilás y el `.exe` sigue con la vieja, **sin error** | El `.rc` de JUCE depende del ícono pero no de `Info.txt` | El bloque del Paso 4.3. Es obligatorio |
| La app y el instalador reportan **versiones distintas** | `getApplicationVersion()` hardcodeado en `Main.cpp`, además de la versión del `.exe` | Paso 4.1: devolver `ProjectInfo::versionString` |
| **Runtime MSVC** | | |
| `LNK4098: defaultlib 'MSVCRTD' conflicts` | Hay un target propio — típicamente el de `juce_add_binary_data` — que no heredó el runtime estático y quedó en `/MD` | Paso 4.4, **opción A** (global, arriba de todo): no enumera targets, así que no se puede olvidar uno |
| `dumpbin /dependents` lista `msvcp140.dll` / `vcruntime140.dll` / `concrt140.dll` | El runtime estático no se aplicó, o se aplicó **después** de crear el target | Paso 4.4. Verificá que el `set()` esté por encima de `juce_add_gui_app` y de `add_subdirectory(JUCE)` |
| **Íconos** | | |
| `Resource update error: Icon file is invalid` | `SetupIconFile` solo acepta `.ico` | Generar `appicon.ico` (Paso 6) |
| El `.ico` del `.exe` sale vacío o sin 48/256 | `ICON_BIG` apunta a un SVG | Usar PNG ≥256 como `ICON_BIG` |
| Íconos de ventana/taskbar borrosos | Se reduce un PNG de 512 al tamaño final en un paso | Rasterizar el SVG al tamaño pedido |
| **Inno Setup / app.json** | | |
| `Error: Unknown constant "99C8..."` al compilar el `.iss` | Inno lee `{GUID}` como constante | `appId` sin llaves en `app.json`; `AppId={{{#AppId}}` en el `.iss` |
| `#error RuntimeLogName is not defined` al compilar el `.iss` | La app no tiene `FileLogger`, pero la plantilla exige el define | Paso 3: declaralo igual en `app.json`; la entrada `[UninstallDelete]` es inocua |
| El reporte del driver dice `Version: 0.0.0.0` para Inno Setup | `ISCC.exe` no lleva `FileVersion` en su recurso de versión | Cosmético, ignorar. La versión real la imprime ISCC en su banner: `Compiler engine version: Inno Setup 7.1.0` |
| **Chequeo de frescura** | | |
| El aviso **nunca** salta, por más que edites los fuentes | El driver solo mira `Source/` y `CMakeLists.txt`. `Test-Path` matchea `Source`↔`src` por case-insensitivity, pero no `app/` ni `code/` | Confirmá la carpeta de fuentes del Paso 1, o agregá la ruta al array `$compiledInputs` del driver |
| El driver avisa "executable looks older than the source" sin cambios de código | Un `git checkout` reescribió los mtimes | `-Force` |
| **Build** | | |
| `cannot open source file "JuceHeader.h"` en todos los headers | `JuceHeader.h` se genera en **tiempo de build**, no al configurar. Pasa después de "Delete Cache and Reconfigure" | Compilar una vez. **No es un problema de CMake** |
| **Sandbox del agente** (no afecta a usuarios reales) | | |
| `Setup was unable to create the directory "...\Temp\is-XXXX.tmp". Error 5: Access is denied.` | Ejecutar el instalador **desde dentro del workspace del agente**. El sandbox del harness corre los procesos en Low Integrity, y Low IL no puede escribir en un `%TEMP%` Medium IL | Copiar el instalador fuera del repo (Escritorio, `Downloads`) y ejecutarlo desde ahí |
| `Start-Process` no lanza la app y el job queda colgado sin salida | `Start-Process` pasa por ShellExecute y el sandbox lo bloquea; además el hijo puede retener el pipe de stdout | Usá `[System.Diagnostics.Process]::Start($exe)` y después `$p.Kill()` desde el mismo proceso |
| **Repositorio** | | |
| `tools/dist/*.ps1` no aparece en `git status` | Regla `dist/` sin anclar en `.gitignore` | `/dist/` (Paso 7) |

---

## Archivos y responsabilidades

| Archivo | Rol | Al replicar |
|---|---|---|
| `packaging/app.json` | **Lo único específico de la app** | Escribir de cero (Paso 3) |
| `packaging/installer.iss` | Plantilla Inno agnóstica; todo llega por `/D` | Copiar; cambiar solo el comentario de cabecera (Paso 2.1) |
| `tools/dist/build-installer.ps1` | Driver: valida, absorbe la versión del `.exe`, invoca ISCC | Copiar y adaptar literales (Paso 2.2) |
| `tools/icons/make-app-icon.ps1` | Genera los 3 assets desde una geometría | Copiar y adaptar literales **+ geometría** (Paso 2.2) |
| `packaging/icon/appicon.{svg,png,ico}` | Assets generados, commiteados | Regenerar (Paso 6) |
| `dist/` | Salida del empaquetado. En `.gitignore` | — |

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
