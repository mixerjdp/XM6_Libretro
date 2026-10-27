# Port fiel de YMFM TypeG

Solicitud: reemplazar completamente la integración YMFM de XM62026 por la de
TypeG, conservando el port ADPCM terminado. Sin Computer Use ni subagentes.
No commit/push nuevo salvo solicitud explícita (el commit anterior e9fcc89
ya se publicó antes de esta tarea).

Referencia SHA256 3d50aab23ce6c8ae1677b1ca8adaa8b0c6c9dabb1c06b98d0f2ea10fe44ac5de.
Referencia de REA/Ghidra sobre xm6g.exe; evidencia de constructor Sound:
ev_a15740e1ec9c15ec31b45c8674e66fc3a6ac92384c612eaab2b9573ca65509b6.
Las exportaciones previas batch500 se reutilizan como evidencia estática.

YMFM TypeG es la rama de objeto 0x5300/vtable 0x1405c4558, seleccionada por
Sound+0x290 == 2. No confundir con el motor global X68Sound de 0x14056ea90.
Core ctor 140570fc0, generate 140570db0, init 140572300, reset 1405723d0,
write 1405724f0, mix 140572580, TimerA 140572960, volume 140572b90.
Objeto: interface+8, chip+0x18, fm_engine+0x20, signal+0x1298 (104 bytes),
trace[8][512]+0x1300, tamaño final 0x5300. Coeficientes e interfaz se obtienen
del PE original. Volumen recibe UI*2, cuantiza int((db-140)*0.2), pow10*.025.
Sampler a cinco tasas; interpolación controla filtro recursivo TypeG.
Prepare inmediato tras KeyOn y TimerA es parte de la implementación.

Implementado: vm/typeg_ymfm.h portable, reemplazo ymfm_opm_engine.cpp,
trace en fm_channel, preparación inmediata, filtro/sampler/ganancia fieles,
soporte de tasas MFC y C API, flags/frecuencia Libretro. OPMIF máscara CT y
CSM de inicio adaptadas a TypeG. Alias legacy raw/direct ya no salta el DAC.
Backup previo sólo local: tools/typeg_audio/build/ymfm_baseline/.

Validado: compare_ymfm.py, 320 comparaciones, 15920 frames,
PCM y State completo (104 bytes+8*512 trazas) exactos, todas las tasas,
ocho algoritmos y filtros on/off. stress_ymfm.py: 269 checkpoints y 18014 frames,
LFO/ruido/CSM, ganancias 0..100, ocho voces y cambios en caliente. Se descubrió
y corrigió que Reset debe conservar trace_index.

Checkpoint completo sin punteros: registros, fase, envolventes, LFO/ruido,
feedback, cachés, máscaras, señales/filtro/trazas y ring de OPMIF. Payload
MFGT v1 y checkpoint YMF1 v1. Lectura del formato anterior conservada.

Verificaciones terminadas:
- Diez casos de checkpoint aislado, continuación exacta PCM/estado.
- Nativo x86/x64: 269 checkpoints originales y nueve continuaciones por arch.
- Core real x64: 28 casos de PCM vs TypeG, checkpoint completo y lectura antigua.
- Migración de OPMIF antiguo sintetizado también en x86/x64.
- Cinco productos Release compilados: MFC Win32, core Win32/x64, Libretro Win32/x64.
- Regresión ADPCM, opciones, recursos y savestates sin fallos.
- Runner único run_tests.ps1 incorpora ambos motores y genera los dos manifiestos.

Evidencia reproducible y hashes: tools/typeg_audio/reports/ymfm_validation.json.
Alcance exacto y comandos: tools/typeg_audio/README.md. Sin GUI, dispositivos de
audio ni CPU/juegos ejecutados. No afirma fidelidad universal al hardware; es
igualdad comprobada contra esta versión de TypeG para los casos registrados.
