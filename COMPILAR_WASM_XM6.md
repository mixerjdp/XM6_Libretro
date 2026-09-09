# XM6 X68000 en WebAssembly — juego4

Fecha de verificación: 2026-08-06.

## Resultado

- Sitio público: https://juego4.140.84.165.65.sslip.io/
- `sitio4.140.84.165.65.sslip.io` queda como alias antiguo y redirige a `juego4`.
- Directorio de despliegue remoto: `/home/ubuntu/sitio4/`.
- Contenedor remoto: `juego4-xm6` (`xm6-sitio4:latest`), activo detrás de Coolify/Traefik.
- Contenido: `I:\sw\Super_Chepi_Bros\out\x68000\chepi_x68000.hdf`.
- El sitio no modifica `juego1`, `juego2` ni `juego3`.
- La actualización se despliega por SSH; no requiere la API de Coolify.

## Paquete local

El directorio `site4/` contiene la página y la configuración de despliegue. Los artefactos están en `site4/www/`:

```text
site4/
  Dockerfile
  docker-compose.yml
  health
  index.html
  nginx.conf
  www/
    xm6_libretro.js
    xm6_libretro.wasm
    system/
      IPLROM.DAT
      CGROM.DAT
      SRAM.DAT
    content/
      chepi_x68000.hdf
```

## Compilación

Se usa Emscripten 3.1.46. El servidor de compilación es ARM64, por lo
que Docker se ejecuta con `--platform=linux/amd64`.

XM6 usa Musashi: todas las compilaciones se hicieron con `C68K=0`. El objetivo Emscripten añadido a `libretro/Makefile.libretro` produce un archivo estático `xm6_libretro.bc`, sin `-ldl` ni `-pthread`. Los archivos C se compilan con `-std=gnu11`.

Para evitar una incompatibilidad de Clang WebAssembly con las instanciaciones explícitas de `vm/ymfm/ymfm_opm.cpp`, YMFM se omite únicamente para `platform=emscripten`; el backend de audio elegido es el XM6/ FMGEN original. La compilación nativa no cambia ese comportamiento.

La fase de enlace de RetroArch usa:

```text
NODE_OPTIONS=--jitless --max-old-space-size=3072
```

Flags de rendimiento de la build WASM publicada:

```text
core compile: -O2 -ffast-math -fomit-frame-pointer
RetroArch link: -O3
```

### 1. XM6 a bitcode

En el servidor ARM:

```bash
ROOT=/home/ubuntu/ra-wasm-build

sudo docker run --rm --platform=linux/amd64 \
  -v "$ROOT:/src" \
  -w /src/XM62026/libretro \
  -e GIT_CONFIG_COUNT=1 \
  -e GIT_CONFIG_KEY_0=safe.directory \
  -e GIT_CONFIG_VALUE_0='*' \
  emscripten/emsdk:3.1.46 \
  bash -lc '
    set -e
    embuilder build zlib
    emmake make -f Makefile.libretro platform=emscripten C68K=0 clean || true
    emmake make -f Makefile.libretro platform=emscripten C68K=0 -j2
    ls -lh xm6_libretro.bc
  '
```

La salida es `XM62026/libretro/xm6_libretro.bc`. El nombre correcto es
`xm6_libretro.bc`, no `xm6_libretro_emscripten.bc`. El `clean` es importante
si cambiaron `C68K`, defines o flags; para un cambio aislado de código se
puede omitir después de una primera compilación correcta.

### 2. Enlazar RetroArch Web

```bash
cp -f "$ROOT/XM62026/libretro/xm6_libretro.bc" \
      "$ROOT/RetroArch/libretro_emscripten.bc"
rm -f "$ROOT/RetroArch/libretro_emscripten.a" \
      "$ROOT/RetroArch/xm6_libretro.js" \
      "$ROOT/RetroArch/xm6_libretro.wasm"

sudo docker run --rm --platform=linux/amd64 \
  -v "$ROOT:/src" \
  -w /src/RetroArch \
  -e GIT_CONFIG_COUNT=1 \
  -e GIT_CONFIG_KEY_0=safe.directory \
  -e GIT_CONFIG_VALUE_0='*' \
  -e NODE_OPTIONS='--jitless --max-old-space-size=3072' \
  emscripten/emsdk:3.1.46 \
  bash -lc '
    set -e
    embuilder build zlib
    emmake make -f Makefile.emscripten LIBRETRO=xm6 -j2 all
    ls -lh xm6_libretro.js xm6_libretro.wasm
  '
```

No se debe subir un `.dll` o `.so` directamente: en WebAssembly RetroArch y
XM6 quedan enlazados en el par `xm6_libretro.js` + `xm6_libretro.wasm`.

## BIOS, SRAM y HDF

XM6 busca los archivos directamente en el directorio del sistema, con nombres en mayúsculas:

```text
/home/web_user/retroarch/userdata/system/IPLROM.DAT  131072 bytes
/home/web_user/retroarch/userdata/system/CGROM.DAT  786432 bytes
/home/web_user/retroarch/userdata/system/SRAM.DAT    16384 bytes
```

El HDF se monta como contenido en:

```text
/home/web_user/retroarch/userdata/content/chepi_x68000.hdf
```

MD5 de los artefactos publicados:

```text
IPLROM.DAT          7FD4CAABAC1D9169E289F0F7BBF71D8E
CGROM.DAT           CB0A5CFCF7247A7EAB74BB2716260269
SRAM.DAT            20FD9F414F0EB8C68B1F02C3033F966D
chepi_x68000.hdf    141F7A339F6A6A3C8B9740C4F2C5C77C
```

## Opciones XM6

La página copia las opciones de `D:\Emulation\Emulators\RetroArch\config\XM6\XM6.opt`, incluyendo CPU a **25 MHz por defecto**, RAM de 12 MB, `exec_to_frame`, FDD0, `fast_floppy`, `render_px68k=enabled`, audio XM6, MIDI GM, ratón y los volúmenes/ecualización nativos.

Las velocidades expuestas por el core son `10mhz`, `12mhz`, `16mhz`,
`22mhz`, `25mhz` y `40mhz`. El valor por defecto del core, del runtime y de
la página WebAssembly es `25mhz`.

El frontend WebAssembly usa además `video_vsync=false`,
`video_refresh_rate=75.0`, `video_swap_interval=1`,
`vrr_runloop_enable=true`, `video_frame_delay=0`,
`audio_sync=false`, `audio_latency=256` y `audio_rate_control=true`.
`video_refresh_rate=75.0` funciona como umbral interno para los modos XM6 de
73.94 Hz; el callback RAF del navegador limita la presentación a un frame por
refresco y el driver de audio mantiene el ritmo del core.

## Teclado y controles táctiles

La página conserva el teclado directo y añade overlay táctil. El mapeo es:

| Control | Tecla enviada |
|---|---|
| Cruceta | Flechas |
| A | X |
| B | Z |
| C | A |
| D | S |
| SELECT | F1 |
| START | Enter |

El cambio de START es exclusivamente de la página WebAssembly: el overlay ahora envía `Enter`, igual que el teclado físico del PC. No cambia el core ni el protocolo Libretro.

## Impacto en builds nativas

La corrección de START no altera las builds Linux ni Windows. Solo modifica `site4/index.html` y el paquete publicado.

La modificación previa de `libretro/Makefile.libretro` sí añadió un objetivo condicionado a `platform=emscripten`: `.bc`, enlace estático, defines Emscripten y exclusión de YMFM únicamente en WebAssembly. En la rama nativa se conservan `.so`, `-ldl` y YMFM. El único cambio común es indicar `-std=gnu11` al compilar el archivo C, sin cambiar el código de emulación ni el comportamiento runtime.

Windows sigue usando `libretro/build_libretro_msvc.bat` y sus DLL existentes no fueron reemplazadas por esta tarea. No es necesario recompilar Linux o Windows por el cambio del botón.

Los botones visibles en el navegador son: `▲`, `▼`, `◀`, `▶`, `A`, `B`, `C`, `D`, `SELECT` y `START`.

## Verificación final

Todos estos recursos devolvieron HTTP 200:

```text
/health                    text/plain, 3 bytes
/index.html                text/html, 21572 bytes
/xm6_libretro.js           application/javascript, 264630 bytes
/xm6_libretro.wasm         application/wasm, 3770179 bytes
/system/IPLROM.DAT         application/octet-stream, 131072 bytes
/system/CGROM.DAT         application/octet-stream, 786432 bytes
/system/SRAM.DAT           application/octet-stream, 16384 bytes
/content/chepi_x68000.hdf  application/octet-stream, 10441728 bytes
```

El arranque en navegador confirmó:

- Lectura de IPLROM y CGROM con los tamaños esperados.
- Backend OPM XM6/FMGEN a 44100 Hz.
- Montaje del HDF en SASI0.
- Salida XRGB8888 y transiciones de vídeo hasta `384x256`.
- `SET_GEOMETRY: 384x256`, aspecto 1.333.
- Opciones XM6 aplicadas, incluido `clock=25mhz`, `ram=12mb` y `audio=XM6`.
- El log del core enumera también las velocidades nuevas `25mhz` y `40mhz`.
- Audio síncrono activo y sin el mensaje `Game FPS > Monitor FPS. Cannot rely on VSync` que desactivaba el límite.

El diagnóstico interno del framebuffer capturó `chepi_x68000-260801-205818.png` y obtuvo:

```text
framebuffer=951x1104
non_black=678063/1049904
mean_luma=25.28
histogram16=621699,283770,2499,34029,4971,18050,0,19145,0,0,0,0,44573,0,21168,0
```

Rendimiento observado en esa ejecución:

```text
60 frames directos del runner: 148.50 ms, 2.475 ms/frame de ejecución
frecuencia del core: 73.94 Hz
periodo efectivo derivado: 13.524 ms/frame
```

El valor de 2.475 ms/frame es el coste de ejecutar 60 frames directamente; el valor de 13.524 ms/frame es el periodo reportado por el timing del core con su ritmo de emulación. La advertencia aislada de RetroArch `Cannot push NULL or empty core path into the playlist` aparece al usar el core embebido y no impide el arranque, el montaje ni el render.

Después del ajuste de velocidad, la prueba registra `Started synchronous audio driver` y ya no registra `Game FPS > Monitor FPS. Cannot rely on VSync`. Eso deja activo el límite del frontend: un callback RAF por refresco del navegador, normalmente 60 Hz, con audio síncrono como respaldo.

## Re-despliegue remoto

En `/home/ubuntu/sitio4/` se puede reconstruir el contenedor y levantarlo con:

```text
sudo docker build -t xm6-sitio4:latest /home/ubuntu/sitio4
sudo docker compose -f /home/ubuntu/sitio4/docker-compose.yml up -d --force-recreate
```

Antes de reconstruir, copiar a `/home/ubuntu/sitio4/www/` el par
`xm6_libretro.js`/`xm6_libretro.wasm`, el `index.html` si cambiaron las
opciones y el HDF actualizado. La configuración usa la red externa
`coolify`, Nginx para servir WASM con su MIME correcto y Traefik para
`juego4.140.84.165.65.sslip.io`.

Hashes SHA-256 de los artefactos publicados el 2026-08-06:

```text
xm6_libretro.js    732c77e88f7e1695fabf58be76d719f70db33ca01f0cc8243ae3fe0f129dda64
xm6_libretro.wasm  0545b92b9851e00d1815f9d5733bba667d9fca30ec46e45d476a96360f7d856e
```
