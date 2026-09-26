# Distribución en Windows: SmartScreen, "editor desconocido" y certificados

Estado: investigación completada — agosto 2026
Aplica a: Convolver (JUCE GUI app Win32, `Convolver.exe`, release de ~8.5 MB)

---

## 1. Qué está pasando realmente

El aviso que viste **no es un antivirus detectando un virus**. Son varios mecanismos
distintos de Windows que se confunden todo el tiempo:

| Mecanismo | Texto típico | Qué lo dispara | Qué lo arregla |
|---|---|---|---|
| **SmartScreen** (Reputación de aplicaciones) | "Windows protected your PC" / "Windows protegió tu PC" → enlace **Más información** → **Ejecutar de todas formas** | El archivo tiene **Mark-of-the-Web (MOTW)** y reputación insuficiente | Quitar el MOTW, o firmar + acumular reputación, o Store |
| **Attachment Manager** | `Open File - Security Warning` — *"The publisher could not be verified"* | El archivo tiene **MOTW** y **no tiene firma válida** | Quitar el MOTW, o firmar |
| **UAC** (Control de cuentas de usuario) | "¿Quieres permitir que esta aplicación...?" — *Editor: Desconocido* | El binario pide elevación y no está firmado | **Solo** un certificado de firma de código |
| **Windows Defender** (antivirus) | "Amenaza encontrada: Trojan/Win32/..." | Heurística ML sobre un binario nuevo/sin firma | Enviar falso positivo a Microsoft |
| **Smart App Control** | "Esta aplicación está bloqueada" | Binario sin firma de una CA del Trusted Root | MSIX en Store, o certificado real |

Por la descripción original ("botón *Avanzado*", "editor desconocido", "podría ser un
virus") el diagnóstico inicial fue **SmartScreen**, que es gratis de evitar y no tiene
nada que ver con Defender ni con antivirus.

### 1.1. Distinguir los dos diálogos

> **Actualización tras ver la captura del diálogo real.** El usuario aportó una imagen
> de `Open File - Security Warning` con el checkbox "Always ask before opening this
> file". Ese diálogo **no es SmartScreen**: es el **Attachment Manager**. La confusión
> es importantísima porque ambos aparecen por el mismo motivo (MOTW) pero tienen
> remedios y gravedad distintos.

Hay **dos** advertencias distintas que la gente confunde. Son mecanismos separados,
se disparan por cosas distintas, y se evitan de forma distinta:

| | **"Open File - Security Warning"** | **SmartScreen** |
|---|---|---|
| Título | `Open File - Security Warning` | `Windows protected your PC` |
| Texto | *"The publisher could not be verified. Are you sure you want to run this software?"* | *"Windows protected your PC — Microsoft Defender SmartScreen prevented an unrecognized app from starting"* |
| Icono | Escudo azul/blanco de Windows | Escudo **naranja con X** (Defender) |
| Autor | **Attachment Manager** (componente local del shell) | **Defender SmartScreen** (veredicto de la nube de Microsoft) |
| Botón por defecto | **Run** (permitir) — hay que pulsar *Cancel* para parar | **Don't run** (bloquear) — el botón resaltado bloquea |
| Fricción | Baja: un clic en *Run* | Alta: hay que buscar un enlace *More info* y luego *Run anyway* |
| Se dispara por | MOTW **+** ausencia de firma válida | MOTW **+** reputación de hash/editor insuficiente |
| Se evita | Quitar MOTW **o** firmar | Quitar MOTW **o** Store/firma+reputación |

La diferencia de **botón por defecto** es la clave práctica de toda esta investigación:

- El diálogo de la imagen (`Open File - Security Warning`) te deja pasar **pulsando el
  botón que ya está resaltado**. Es el shell diciendo *"no puedo verificar quién hizo
  esto, pero tú decides"*.
- SmartScreen pone *"Don't run"* como opción por defecto y obliga a buscar el enlace
  escondido para continuar. Es un **veredicto de reputación** de Microsoft, no una
  decisión local. De ahí que se sienta mucho más grave.

Además, el de la imagen **tiene el checkbox "Always ask before opening this file"**,
que es justamente el control de usuario del Attachment Manager.

#### Lo que NO se puede hacer con esto

Existe una directiva del sistema (*Windows Components → Attachment Manager*) para
desactivar ese diálogo. **No la incluimos como recomendación** por dos razones:

1. Requiere tocar el registro del PC del usuario (`Policies\Attachments`), no es algo
   que tu app o tu instalador deban hacer.
2. **No resuelve SmartScreen.** Son mecanismos distintos, y desactivar el Attachment
   Manager deja el bloqueo azul intacto.

El dato que sí importa de esta investigación: la
[documentación de Microsoft](https://learn.microsoft.com/en-us/archive/blogs/askie/how-to-bypass-the-security-warning-unknown-publisher-with-the-checkbox-always-ask-before-opening-this-file)
confirma que **quitar el MOTW es exactamente el camino correcto**, y que la única
alternativa real es firmar el binario.

### La causa raíz: Mark-of-the-Web

Cuando descargas un `.exe` desde un navegador, correo o Drive, Windows le pega un
flujo NTFS alternativo llamado `Zone.Identifier`:

```
[ZoneTransfer]
ZoneId=3            <- 3 = Internet, 4 = Restricted Sites
ReferrerUrl=https://...
```

Mientras exista ese stream, Windows trata el archivo como "contenido de Internet"
y SmartScreen interviene. Puedes comprobarlo tú mismo:

```powershell
Get-Content .\Convolver.exe -Stream Zone.Identifier
```

### Lo que la firma cambia (y lo que no)

Firmar **no** elimina el aviso la primera vez. Solo cambia lo que dice:

- **Sin firma** → *"Editor desconocido"* / "Unknown publisher"
- **Con firma OV nueva** → *"Aplicación no reconocida"* pero con tu nombre verificado
- **Con reputación acumulada** → sin aviso

Esto está confirmado en la propia documentación de Microsoft
([SmartScreen reputation](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)):
la reputación se acumula **por hash de archivo** y **por certificado**. Sin firma,
cada versión nueva empieza de cero.

---

## 2. Los enlaces que te pasaron

| Enlace | Qué es |
|---|---|
| [partner.microsoft.com](https://partner.microsoft.com/en-US/) | Partner Center: el panel donde se publica una app en la Microsoft Store |
| [learn.microsoft.com/.../windows/apps/publish](https://learn.microsoft.com/es-es/windows/apps/publish/) | Documentación de publicación de apps Windows |

Quien te los pasó iba en la dirección correcta: **la Microsoft Store es la única
vía gratuita que elimina SmartScreen por completo** (§4, opción C). Pero conviene
saber que Partner Center también sirve para cuentas de empresa y para servicios de
pago, así que no es automáticamente gratis solo por entrar ahí.

---

## 3. Lo que ya NO funciona (evita perder dinero y tiempo)

Antes de las soluciones, las trampas conocidas:

| Idea | Veredicto |
|---|---|
| **Autofirmar con un certificado propio** (`New-SelfSignedCertificate` + `signtool`) | ❌ **No sirve.** Microsoft lo dice explícitamente: mismo comportamiento que sin firma. Windows desconoce esa CA raíz. Solo vale para desarrollo local o despliegue empresarial con GPO/Intune que distribuya la CA. |
| **Comprar un certificado EV** (400+ USD/año) | ❌ **Ya no sirve.** Microsoft eliminó el bypass de SmartScreen para EV **en 2024**. Hoy EV y OV se comportan igual frente a SmartScreen. Pagar el premium por esto ya no está justificado. |
| **"Enviar el archivo a Microsoft para que lo aprueben"** | ❌ No existe ese trámite para consumidores. La reputación se construye sola con volumen de descargas. El [portal WDSI](https://www.microsoft.com/en-us/wdsi/filesubmission) es solo para **falsos positivos de antivirus**, no para SmartScreen. |
| **Cambiar el nombre del archivo / recompilar con otro linker** | ❌ No hay truco de nombres. Cada hash nuevo empieza de cero. |
| **Certificado OV "barato"** (150–300 USD/año) | ⚠️ Técnicamente correcto, pero **igual muestra aviso** en las primeras descargas hasta acumular reputación (semanas y cientos de instalaciones limpias). |

---

## 4. Las opciones reales, ordenadas por coste

### A. Quitar el Mark-of-the-Web  ·  coste 0  ·  esfuerzo mínimo

Para la máquina que **ya tiene** los archivos. Es lo que resuelve tu caso inmediato.

**Manual:** clic derecho en `Convolver.exe` → Propiedades → abajo del todo,
marcar **"Desbloquear"** → Aplicar. (Si no aparece esa casilla, el archivo no
tenía MOTW y el problema es otro.)

**Script incluido en este repo:**

```powershell
# Ver qué está mal, sin tocar nada
.\tools\dist\unblock-distribution.ps1 -CheckOnly

# Desbloquear toda una carpeta de distribución
.\tools\dist\unblock-distribution.ps1 -Path "D:\Convolver-0.1.0-win64"
```

El script además informa del estado de SmartScreen, Smart App Control y Defender,
y del estado Authenticode de cada payload — para que no haya ambigüedad sobre cuál
de los tres mecanismos está actuando. Está probado: detecta `ZoneId=3` y lo elimina.

**Límite:** solo arregla la máquina donde ejecutas el script. Quien descargue el
`.exe` de la web genera un MOTW nuevo. **Por eso esto no es una solución de
distribución, es un parche para tu PC de pruebas.**

---

### B. Distribuir con instalador  ·  coste 0  ·  **la mejor relación esfuerzo/beneficio**

Esta es la recomendación principal si el objetivo es "que en el otro PC no me
bloquee".

**El truco:** el "bloqueo" solo aparece **una vez**, cuando el usuario ejecuta el
instalador. Los archivos que el instalador **extrae** en `Program Files` no tienen
MOTW. A partir de ahí, `Convolver.exe` abre limpio, siempre, en todos los PC.

Comparado con mandar un ZIP o el `.exe` suelto:

| Entrega | Veces que el usuario sufre el aviso |
|---|---|
| `.exe` suelto por correo/Drive | Cada vez que lo descarga y en cada versión nueva |
| ZIP | Igual, y además DOS veces (el ZIP y el `.exe` dentro) |
| **Instalador** | **Una sola vez, la primera** |

Herramientas gratuitas y suficientes:

- **Inno Setup 6** — [jrsoftware.org/isinfo.php](https://jrsoftware.org/isinfo.php) — licencia libre, script `.iss` simple, el más usado para apps pequeñas.
- **NSIS** — [nsis.sourceforge.io](https://nsis.sourceforge.io/) — más potente, curva mayor.
- **WiX Toolset** — [wixtoolset.org](https://wixtoolset.org/) — MSI real, integrable con CMake vía **CPack**. Es la opción más "de build" y encaja con este repo (CMake 4.3.1 ya instalado; solo falta `wix`).

Para Convolver, Inno Setup es el camino corto: un `.iss` de ~40 líneas que copia
`Convolver.exe` a `{app}`, crea accesos directos, y registra un desinstalador.

> **Aviso sobre ZIP:** repartir un ZIP **empeora** las cosas: hay que desbloquear el
> ZIP antes de extraer (o los archivos extraídos heredan el MOTW). Si aun así
> quieres ZIP, incluye dentro un `LEEME.txt` con la instrucción de Propiedades →
> Desbloquear, o ejecuta el script de §A.

#### Secuencia exacta de avisos con un instalador sin firmar

Esta es la respuesta a *"¿qué bloqueo queda si empaqueto en un instalador?"*.
Asumiendo que el usuario descarga el instalador desde el navegador (por lo tanto con
MOTW) e instala en `Program Files` (por lo tanto con elevación):

| # | Momento | Qué ve el usuario | Se puede evitar gratis |
|---|---|---|---|
| 1 | Ejecuta el instalador | **SmartScreen**: "Windows protected your PC" | ❌ No — el instalador también tiene MOTW |
| 2 | Confirma | **Attachment Manager**: "publisher could not be verified" | ❌ No, sin firmar |
| 3 | El instalador pide permisos | **UAC**: "Editor: Desconocido" | ✅ **Sí** — ver abajo |
| 4 | Copia de archivos | Nada | — |
| 5 | **Primera ejecución de Convolver** | **Nada. Limpio.** | ✅ Ya resuelto |
| 6 | Cada ejecución posterior | **Nada. Limpio.** | ✅ Ya resuelto |

**El punto 5-6 es la propiedad clave, y conviene entender por qué funciona:**

El MOTW es un **flujo NTFS alternativo** (`Zone.Identifier`). No es parte del
contenido del archivo, y **no se copia** cuando un programa escribe un archivo nuevo.
El instalador no "copia" `Convolver.exe` con el Explorador — lo **extrae de dentro de
su propio paquete y lo escribe como archivo nuevo** en `Program Files`. Ese archivo
recién creado no tiene MOTW.

Resultado: el usuario sufre los avisos **una sola vez, en la instalación**, y la
aplicación arranca limpia para siempre. Eso es exactamente lo que pediste: *"no me
importa un mensaje, pero que no me bloquee"*.

**Sobre el punto 3 (UAC):** se elimina gratis instalando **por usuario** en lugar de
por máquina. Un instalador per-user escribe en
`%LOCALAPPDATA%\Programs\Convolver\` en vez de `C:\Program Files\`, y **no pide
elevación** → no hay diálogo de UAC en absoluto. Inno Setup lo hace con
`PrivilegesRequired=lowest`.

| Diseño del instalador | Avisos que quedan |
|---|---|
| Per-machine (`Program Files`) | SmartScreen + Attachment Manager + **UAC** |
| **Per-user (`%LOCALAPPDATA%\Programs`)** | SmartScreen + Attachment Manager |
| MSIX en Microsoft Store | **Ninguno** |

Las dos primeras filas son idénticas en fricción salvo por el UAC, que es el aviso
más "grave" de los tres porque menciona *Editor: Desconocido* en la pantalla segura
del escritorio. Por eso `PrivilegesRequired=lowest` es la opción recomendada para
Convolver: la app no necesita permisos de administrador para convolucionar WAVs.

> **Contrapartida del per-user:** la app se instala solo para el usuario que la
> ejecutó, y no aparece en `Program Files`. Para una herramienta de audio personal o
> de equipo pequeño es irrelevante; si algún día necesitas instalación para todos los
> usuarios de la máquina, vuelves a per-machine y aceptas el UAC.

---

### C. Microsoft Store con MSIX  ·  coste 0  ·  **la única vía gratuita a cero avisos**

Es exactamente lo que apuntan los enlaces que te pasaron, y es la recomendación
oficial de Microsoft.

**Por qué funciona gratis:** cuando pases la certificación, **Microsoft vuelve a
firmar tu paquete con su propio certificado**. El resultado hereda la reputación
completa de Microsoft. Cero avisos de SmartScreen, siempre.

**Coste real: 0 €.** La nota de Microsoft es explícita
([abrir cuenta de desarrollador](https://learn.microsoft.com/en-us/windows/apps/publish/partner-center/open-a-developer-account)):

> *"With the new onboarding experience, there are **no registration fees** for either
> account type, so you can create your developer account and start publishing at no
> cost."*

Eso sí: **hay que entrar por [storedeveloper.microsoft.com](https://storedeveloper.microsoft.com)**.
Si entras directamente por Partner Center, verás el flujo antiguo con la cuota.
Requiere verificación de identidad (documento oficial + selfie).

**Qué implica técnicamente:**

| Punto | Detalle |
|---|---|
| Cuenta | Individual, gratis, verificación de identidad |
| Revisión | Certificación de la Store (unos días, con posibilidad de rechazo y reenvío) |
| Distribución | **Solo vía Store**. No puedes ofrecer además descarga directa del MSIX sin firmar, o pierdes el beneficio |
| Actualizaciones | Cada versión nueva pasa por certificación |
| Empaquetado | `.msix` + `AppxManifest.xml`. Se hace con `MakeAppx.exe` (Windows SDK) o Visual Studio |
| Nombre | Hay que **reservar el nombre** en Partner Center; el `Identity/@Name` del manifiesto debe coincidir o la subida falla |

En este repo **falta `makeappx.exe`** (no está instalado el Windows SDK completo),
así que esta vía requiere instalar tooling antes de poder intentarla.

#### C.1. Qué podría fallar — análisis sobre el código real

Esto es una auditoría de los riesgos concretos de empaquetar Convolver como MSIX,
verificada contra el código de `Source/` y contra el JUCE de `C:\JUCE`.

**Regla fundamental del contenedor MSIX** (según la
[documentación de containerización](https://learn.microsoft.com/en-us/windows/msix/msix-containerization-overview)):

> *"When you install an MSIX package, Windows places the app's files in a protected
> location (`C:\Program Files\WindowsApps\`) that **the app itself cannot modify**."*
>
> *"Write inside the package — **Not allowed**. The package is read-only."*

Y según [cómo se ejecutan las apps empaquetadas](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes):

> *"Writes to files/folders in the app package **aren't allowed**."*

##### 🔴 Bloqueante: el log de arranque

`Source/Main.cpp:15-18` ejecuta esto **en el constructor de la aplicación**, es decir
en el arranque:

```cpp
auto exeFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
auto logFile = exeFile.getParentDirectory().getChildFile ("convolver_runtime.log");
fileLogger = std::make_unique<juce::FileLogger> (logFile, "Convolver Runtime Log", 0);
```

Bajo MSIX, `getParentDirectory()` del ejecutable es
`C:\Program Files\WindowsApps\<paquete>\`, que es **de solo lectura**. Escribir
`convolver_runtime.log` ahí no está permitido.

**¿Crashea la app?** No. Verificado leyendo JUCE:

- `FileOutputStream::openHandle()` (`juce_Files_windows.cpp:429-450`) no lanza
  excepción: si `CreateFile` falla, solo hace `status = getResultForLastError()`.
- `FileLogger` (`juce_FileLogger.cpp:46-47`) hace `file.create()` sin comprobar el
  resultado.
- `FileLogger::logMessage` (`juce_FileLogger.cpp:65-66`) crea un `FileOutputStream`
  nuevo en **cada** mensaje y sigue adelante.

Así que la app **funciona, pero el log nunca se escribe**. Es un fallo silencioso: no
rompe MSIX, pero **elimina toda la capacidad de diagnóstico** que este proyecto usa
para depurar (ver `.github/copilot-instructions.md`, sección *Where to look for problems*).

**La corrección es de una línea, y además es un bug preexistente**: con un instalador
per-machine en `Program Files`, un proceso sin elevación tampoco puede escribir ahí.
Es decir, **el log ya está roto hoy** en cualquier despliegue real, no solo bajo MSIX.
JUCE ya tiene la solución:

```cpp
// en lugar de construir FileLogger con una ruta junto al .exe:
fileLogger.reset (juce::FileLogger::createDefaultAppLogger (
    "Convolver", "convolver_runtime.log", "Convolver Runtime Log"));
```

`FileLogger::getSystemLogFileFolder()` (`juce_FileLogger.cpp:114-121`) devuelve
`File::userApplicationDataDirectory` en Windows → `%APPDATA%`, que es una ubicación
de usuario y por lo tanto escribible y redirigible bajo MSIX. Contrapartida: el
`run_capture_assertions.ps1` actual busca el log junto al ejecutable y habría que
actualizar su ruta.

##### 🟡 No bloqueante: los diálogos de archivo

Verificado que **no es el problema que se suele suponer**:

- `Source/MainComponent.cpp:68, 102, 135` usan `juce::FileChooser` sin pasar
  `useOSNativeDialogBox`, por lo que se usa el valor por defecto. En
  `juce_FileChooser.h:109` el parámetro es `useOSNativeDialogBox` y el constructor
  (`juce_FileChooser.cpp:119`) hace `useNativeBox && isPlatformDialogAvailable()`.
- En Windows el camino nativo es `juce_FileChooser_windows.cpp`, que usa
  **`IFileOpenDialog` / `IFileDialog`** (línea 183, 392), la API moderna compatible
  con MSIX. Filtra por `FOS_PICKFOLDERS` (línea 198) para directorios.
- El código **nunca** pide archivos y directorios a la vez, así que no cae en el
  camino no nativo de `juce_FileChooser.cpp:227`.

Regla práctica: **si el usuario eligió el archivo a través del selector, la app puede
leerlo y escribir en esa carpeta.** Como Convolver solo toca archivos que el usuario
seleccionó explícitamente, el acceso a ficheros no debería ser un obstáculo. No hace
falta `broadFileSystemAccess` ni reescribir los selectores.

##### 🟢 Sin riesgo: otras áreas

| Área | Estado |
|---|---|
| Registro de Windows | La app no escribe nada. Sin virtualización de registro que estorbe |
| Dispositivos de audio | `juce_audio_devices` está linkado pero la app es procesamiento offline; no abre hardware |
| Escritura de salida | `ConvolutionEngine.cpp:139-141` escribe en la carpeta elegida por el usuario → permitido |
| DPI / manifest | JUCE ya declara DPI awareness |
| Runtime MSVC | Enlazado estáticamente (`CMakeLists.txt:41-44`); el paquete se autoabastece |
| `DwmSetWindowAttribute` | `Main.cpp:76-78`; opera sobre la ventana propia de la app, sin conflicto |
| Auto-actualización | No existe auto-update que pudiera chocar con el modelo atómico de MSIX |

##### 🟡 Riesgos de la revisión (certificación), no técnicos

Microsoft publica los [motivos frecuentes de fallo de certificación](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/resolve-submission-errors).
Los que aplican aquí:

| Requisito | Impacto para Convolver |
|---|---|
| **Windows App Certification Kit (WACK)** | Hay que instalarlo y pasarlo. Es trabajo real, no un trámite |
| **No debe crashear sin conexión a red** | Convolver es offline, así que debería pasar sin cambios |
| **Sin secciones incompletas ni funciones rotas** | La ventana de Help debe estar completa |
| **Política de privacidad (URL)** | **Probablemente obligatoria**: la app accede a archivos del usuario (WAVs) |
| **Age ratings** | Cuestionario obligatorio |
| **Descripción fiel de la app** | Redacción en inglés, capturas, etc. |
| **Nombre reservado** | "Convolver" es un nombre genérico; puede estar ya reservado por otro |
| **Reputación de la Store** | La certificación suele tardar días, y **cada actualización vuelve a pasar por ella** |

El último punto es el coste oculto más importante: **cada release nueva espera revisión.**
Para una app personal o de equipo chico, eso rompe el ciclo de iteración.

##### Veredicto

| | |
|---|---|
| **Bloqueantes técnicos** | **Uno**: el log de arranque. Arreglable con una línea, y conviene arreglarlo igual |
| **Bloqueantes de proceso** | Certificación + WACK + política de privacidad + reserva de nombre + espera por cada release |
| **Tooling faltante** | `makeappx.exe` (Windows SDK), WACK, assets del manifiesto (logos/tiles), `.msixupload` |
| **Lo que NO es problema** | Los selectores de archivo nativos, el registro, el audio, la escritura de salida |

En resumen: **técnicamente MSIX es viable con un cambio de una línea.** El verdadero
coste no es el código, es el proceso: certificación, WACK, privacidad, y esperar
revisión en cada versión. Por eso la recomendación sigue siendo **B (instalador)**
salvo que quieras publicar al público general.

**Recomendación:** ve por **B (instalador)** ahora, y considera **C (Store)** solo si
quieres publicar la app al público general. Para "que no me bloquee en el otro PC de
la oficina", la Store es un cañón para matar un mosquito.

---

### D. Azure Artifact Signing (antes Trusted Signing)  ·  ~10 USD/mes

Lo que Microsoft **recomienda** para distribución fuera de la Store
([opciones de firma](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)).
Sin token USB, se integra en CI/CD. Pero ojo:

- **No da reputación instantánea.** Aviso en las primeras descargas, igual que OV.
- **Restricción geográfica:** organizaciones en EE.UU., Canadá, UE y Reino Unido.
  **Desarrolladores individuales: solo EE.UU. y Canadá.** Si estás en otro país como
  individuo, no puedes usarlo.
- No es "gratis", aunque es lo más barato del mercado con firma real.

---

### E. SignPath Foundation  ·  coste 0  ·  solo si el proyecto es open source

[signpath.io](https://signpath.io) ofrece firma de código **gratuita** para proyectos
open source que cumplan los requisitos, con certificado de nivel OV gestionado.

**Condiciones:** el proyecto tiene que ser realmente open source (licencia OSI,
repositorio público, build reproducible). Si Convolver es privado, no aplica.
Si lo abres, es una vía gratuita y legítima — pero igual muestra aviso al principio,
porque OV tampoco da reputación instantánea.

---

### F. Distribución por red local o USB  ·  coste 0

Mencionado por completitud, porque técnicamente funciona y a veces es la respuesta
correcta:

- Un archivo copiado a un USB desde tu PC y luego al PC destino **conserva el MOTW**
  → seguirá avisando. Hay que desbloquear en destino (§A).
- Un archivo ejecutado **directamente desde un recurso de red** suele quedar en la
  zona *Intranet*, y SmartScreen normalmente no avisa en la zona intranet.

Es aceptable para 2–3 PC de confianza; no escala y es frágil (depende de directivas
de red que en entornos gestionados pueden prohibirlo).

---

## 5. Si además Defender dijo "virus"

Eso es otra película. SmartScreen **nunca** dice "podría ser un virus"; Defender sí.
Si en el PC destino Defender marcó el archivo:

1. Es un **falso positivo heurístico**, muy común con ejecutables nuevos sin firma.
2. El envío de falsos positivos es aquí: **[microsoft.com/wdsi/filesubmission](https://www.microsoft.com/en-us/wdsi/filesubmission)**
   ([documentación](https://learn.microsoft.com/en-us/defender-endpoint/defender-endpoint-false-positives-negatives)).
3. Factores que **pueden** aumentar la tasa de falsos positivos y conviene revisar
   en este repo. Ninguno es una causa demostrada por sí solo — la evidencia pública
   apunta a que el factor dominante es simplemente *binario nuevo, sin firma y con
   poca prevalencia* — pero son los patrones que la heurística ML mira:

   - **Runtime MSVC estático** — `CMakeLists.txt:41-44` fija
     `MSVC_RUNTIME_LIBRARY = MultiThreaded` (`/MT`). El CRT estático es un patrón
     frecuente en muestras empaquetadas, así que merece la pena probar `/MD`
     (dinámico) y comparar. Contrapartida: requiere el VC++ Redistributable en
     destino, que un instalador (§B) puede incluir o instalar de forma silenciosa.
   - **Optimización real en Release** — `juce::juce_recommended_config_flags` ya
     aporta los flags recomendados; verifica simplemente que la configuración que
     distribuyes es `Release` y no un build `Debug` (el árbol actual tiene
     `Convolver_artefacts\Debug\Convolver.exe` a 27 MB frente a los 8.5 MB de
     `Release`; distribuir el Debug es la peor opción posible).
   - **Metadatos de versión completos** (CompanyName, ProductName, FileVersion,
     LegalCopyright). `juce_add_gui_app` los genera a partir de `PRODUCT_NAME`,
     `COMPANY_NAME` y `VERSION` (`CMakeLists.txt:10-15`); comprueba que aparecen
     rellenos en Propiedades del `.exe`.
   - **Un icono propio**, no el genérico de JUCE.
   - **Baja prevalencia**: un binario que solo existe en 3 PC del mundo tiene cero
     reputación por definición. Esto se corrige distribuyendo más, no compilando distinto.

Estos ajustes **no eliminan SmartScreen** y no garantizan nada frente a Defender,
pero reducen superficie para que un AV marque el archivo.

---

## 6. El caso especial: Smart App Control

Hay una tercera barrera que puede aparecer en un Windows 11 **recién instalado** y
que ninguna de las soluciones gratuitas de arriba resuelve:
[Smart App Control](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/overview).

> *"Malware, Potentially Unwanted Apps (PUA), and unknown, **unsigned code are
> blocked by default**."*

A diferencia de SmartScreen, **no pregunta** — bloquea directamente, y **el MOTW es
irrelevante**. Solo deja pasar binarios que Microsoft reconoce o que están firmados
por una CA del Trusted Root Program.

**Cómo comprobarlo en el PC destino:**
Configuración → Privacidad y seguridad → Seguridad de Windows → **Control de
aplicaciones y navegador**. Si aparece la sección *Smart App Control*:
- **Activado (On)** = modo enforcement → necesitas Store (§C) o certificado real
- **Evaluación (Evaluation)** = aún observando
- **Desactivado (Off)** = no aplica

También por registro:

```powershell
Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -Name VerifiedAndReputablePolicyState
# 0 = Off, 1 = Enforcement (bloquea), 2 = Evaluation
```

Nota importante: Smart App Control **solo se puede activar en una instalación limpia
de Windows**, y una vez desactivado no se puede volver a activar sin reinstalar. Es
decir: en el PC destino, si está activo, **desactivarlo es irreversible** salvo
reinstalación. Es una decisión del dueño del PC, no algo que debas forzar desde el
instalador.

**En tu máquina de desarrollo está `Off`** (verificado), por eso ahí nunca lo verás.

---

## 7. Recomendación concreta para Convolver

Dado que el objetivo es "que en otro PC no me bloquee" y el presupuesto es 0 €:

1. **Ahora mismo, para el PC que ya falló** — usa `tools\dist\unblock-distribution.ps1`
   (o el checkbox *Desbloquear* en Propiedades). Resuelve el síntoma en 10 segundos.
2. **Como solución real de distribución** — empaqueta con **Inno Setup** (§B). Un
   `.iss` de ~40 líneas. El usuario ve el aviso **una vez**, instala, y nunca más.
   Es la respuesta correcta a "no me importa un mensaje, pero que no me bloquee".
3. **Si el PC destino tiene Smart App Control activo** — entonces sí necesitas la
   Store (§C, gratis) o un certificado. Verifica esto **antes** de invertir esfuerzo,
   porque cambia por completo la solución.
4. **No compres nada.** Ni EV (ya no sirve), ni OV (igual avisa), ni autofirmado
   (no sirve). La información que te dieron sobre "tiene que ver con certificados"
   es correcta a medias: los certificados cambian lo que **dice** el aviso de UAC,
   pero SmartScreen es un problema de **procedencia y reputación**, no de firma.

### Lo que NO hay que hacer

- No autofirmar y decirle al usuario que instale tu CA raíz "para que no avise".
  Es lo mismo que enseñarle a ignorar avisos de seguridad.
- No decirle al usuario que desactive SmartScreen globalmente. Es desproporcionado
  y en muchos equipos gestionados ni siquiera es posible.
- No repartir un ZIP sin instrucciones: garantiza el aviso al menos dos veces.

---

## 8. Resumen en una tabla

Avisos que ve el usuario, desglosados por mecanismo. `SM` = SmartScreen,
`AM` = Attachment Manager ("publisher could not be verified"), `UAC` = elevación.

| Opción | Coste | SM | AM | UAC | Después de instalar | Resuelve Smart App Control | Esfuerzo |
|---|---|---|---|---|---|---|---|
| Sin firma (estado actual) | 0 | Sí | Sí | — | Sí, en cada versión | No | — |
| Autofirmado | 0 | Sí | Sí | Sí | Sí | No | Bajo, inútil |
| **Quitar MOTW (script §A)** | **0** | **No** | **No** | — | No, en esa máquina | No | Mínimo |
| **Instalador per-user (§B)** | **0** | **Sí** | **Sí** | **No** | **Limpio** | No | Bajo-Medio |
| Instalador per-machine (§B) | 0 | Sí | Sí | Sí | Limpio | No | Bajo-Medio |
| **Microsoft Store MSIX (§C)** | **0** | **No** | **No** | **No** | **Limpio** | **Sí** | Medio-Alto |
| SignPath Foundation (§E) | 0 (si OSS) | Sí | No | No | Sí, hasta acumular | Sí | Medio |
| Azure Artifact Signing (§D) | ~10 USD/mes | Sí | No | No | Sí, hasta acumular | Sí | Medio |
| Certificado OV | 150–300 USD/año | Sí | No | No | Sí, hasta acumular | Sí | Medio |
| Certificado EV | 400+ USD/año | Sí | No | No | Sí, hasta acumular | Sí | Medio |

Lectura de la tabla para tu caso: **la fila "Instalador per-user" es la respuesta**.
Cuesta 0, el usuario ve los avisos **una sola vez** durante la instalación, y la
aplicación queda limpia para siempre. Sin UAC, sin certificado, sin Store.

---

## 9. Auditar la firma de un .exe o de una app instalada

Herramienta: `tools/dist/inspect-signature.ps1`

Sirve para responder *"¿y esa app cómo está firmada?"* sobre cualquier binario o
carpeta de aplicación instalada. Útil para aprender por comparación: correrlo sobre
software conocido y ver qué hizo cada proyecto.

```powershell
# Un binario
.\tools\dist\inspect-signature.ps1 -Path "C:\Program Files\Everything 1.5a\Everything64.exe"

# Una app instalada entera (recursivo)
.\tools\dist\inspect-signature.ps1 -Path "C:\Program Files\SomeApp" -MaxFiles 20

# Incluyendo catálogos .cat (firma por catálogo, típica en drivers)
.\tools\dist\inspect-signature.ps1 -Path "C:\Program Files\SomeApp" -ScanCatalogs
```

### Qué informa

| Dato | De dónde sale |
|---|---|
| `status` — `Valid` / `NotSigned` / `HashMismatch` / `UnknownError` | `Get-AuthenticodeSignature` |
| `method` — la clasificación (Store, CA comercial, autofirmado, sin firma) | cadena de certificados + heurística de CA conocida |
| `root CA` — la autoridad raíz real | cadena construida hasta la raíz |
| `chains to trusted root` — ¿esa raíz está en el almacén local? | `X509Chain` |
| `timestamped` — ¿tiene sellado de tiempo? | `TimeStamperCertificate` |
| `signer cert expired` | validez del certificado firmante |
| `CompanyName` — lo que UAC y Attachment Manager muestran como editor | recurso de versión del PE |
| Provenance MSIX / Store | `Get-AppxPackage` sobre la ruta instalada |

### El hallazgo importante: los certificados caducados son NORMALES

Esto es contraintuitivo y es la razón por la que este punto está documentado.

Un certificado de firma de código vive típicamente **1 a 3 años**. Eso significa que
**la mayoría del software comercial bien firmado que tenés instalado tiene el
certificado del firmante YA CADUCADO**, y su firma sigue siendo perfectamente válida.
El **sellado de tiempo (timestamping)** es lo que lo hace posible: prueba que el
binario se firmó *mientras el certificado aún era válido*, y eso vale para siempre.

Verificado contra software real de este equipo:

| Binario | Estado | Certificado del firmante | Sello de tiempo |
|---|---|---|---|
| `Everything64.exe` | `Valid` | **Caducado 2025-03-17** | Sí → firma válida |
| `git.exe` | `Valid` | **Caducado** | Sí → firma válida |
| `WinRAR.exe` | `Valid` | **Caducado** | Sí → firma válida |
| `notepad++.exe` | `Valid` | Vigente | Sí |
| `foobar2000.exe` | `NotSigned` | — | — |
| `qbittorrent.exe` | `NotSigned` | — | — |

**Consecuencia práctica para el script:** NO hay que usar `Test-Certificate` ni
`X509Chain` "a fecha de hoy" para decidir si una firma es buena. Producen un diluvio
de **falsos negativos** — yo mismo los obtuve en la primera versión de este script y
reportaba `Trusted: False` en binarios de DigiCert/Sectigo perfectamente válidos.

La implementación correcta, ya aplicada:
1. La validez de la firma la dicta `Signature.Status` (que sí considera el sello).
2. La confianza en la CA se comprueba construyendo la cadena con
   `X509VerificationFlags.IgnoreNotTimeValid`, que responde la pregunta correcta:
   *"¿termina esta cadena en una raíz de confianza?"* — sin importar que el
   certificado firmante haya caducado.

### Qué NO puede determinar

| Pregunta | Por qué no |
|---|---|
| ¿Aparecerá SmartScreen al descargarlo? | La reputación es una señal **en la nube por hash**, no una propiedad del archivo. Un binario `Valid` recién compilado igual muestra el aviso |
| ¿Cuántas descargas tiene? | Esa métrica no es pública |
| ¿Firmó con token hardware o con HSM en la nube? | El certificado resultante es idéntico; no se puede distinguir desde el binario |
| ¿Es malicioso? | Firma válida ≠ seguro. El malware también se firma (y eso quema el certificado) |

El script imprime ese primer punto como recordatorio al final de cada informe, justo
para que nadie confunda "firma válida" con "no va a mostrar el cartel".



- [SmartScreen reputation for Windows app developers](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation) — reputación, tipos de certificado, qué esperar
- [Code signing options for Windows app developers](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options) — comparativa oficial de costes
- [Steps to open a developer account](https://learn.microsoft.com/en-us/windows/apps/publish/partner-center/open-a-developer-account) — confirma que no hay cuota de registro
- [App package requirements for MSI/EXE app](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msi/app-package-requirements) — por qué MSI/EXE en Store no es gratis pero MSIX sí
- [Smart App Control overview](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/overview) — la tercera barrera
- [Address false positives/negatives in Microsoft Defender](https://learn.microsoft.com/en-us/defender-endpoint/defender-endpoint-false-positives-negatives) — envío de falsos positivos
- [Microsoft Security Intelligence submission portal](https://www.microsoft.com/en-us/wdsi/filesubmission) — reportar falso positivo de antivirus
- [SignPath Foundation](https://signpath.io) — firma gratuita para open source
- [MSIX containerization overview](https://learn.microsoft.com/en-us/windows/msix/msix-containerization-overview) — el paquete es de solo lectura: base del bloqueante del log
- [Understanding how packaged desktop apps run on Windows](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes) — tabla de operaciones de fichero permitidas/prohibidas
- [Resolve submission errors for MSIX app](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/resolve-submission-errors) — motivos frecuentes de fallo de certificación y WACK
- [How to bypass the security warning "Unknown Publisher" with the checkbox "Always Ask Before Opening this File"](https://learn.microsoft.com/en-us/archive/blogs/askie/how-to-bypass-the-security-warning-unknown-publisher-with-the-checkbox-always-ask-before-opening-this-file) — confirma que el diálogo de la imagen es Attachment Manager y que las dos salidas son quitar el MOTW o firmar
- [IAttachmentExecute (shobjidl_core.h)](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iattachmentexecute) — la API que produce el diálogo "Open File - Security Warning"
- [Policy CSP - ADMX_AttachmentManager](https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-admx-attachmentmanager) — directivas de Attachment Manager, para referencia (no recomendadas aquí)
