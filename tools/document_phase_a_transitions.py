"""Retain completed graphical evidence and append its result without erasing history."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
result = json.loads((root / "logs/runtime/phase-a-transition-progress.json").read_text())
assert result["phase_a"] == "CLOSED"
for transition in ("resize", "windowed_to_fullscreen", "fullscreen_to_windowed"):
    assert result[transition]["status"] == "PASS"
    assert (root / "logs/runtime" / result[transition]["log"]).is_file()
    assert (root / "logs/runtime" / result[transition]["screenshot"]).is_file()

heading = "## Cierre de fase A — transiciones verificadas, 2026-10-02"
detail = """
COMPLEMENTARY_OPENGL = PASS. PHASE_A = CLOSED.

La continuación reutilizó el JAR phase-a-fix sin reconstruir SR, Wisteria ni el core.
JAR SHA-256: a0a345486e59968dde0b60812e63e1420223ca12e07d758b2af281cfd72691b9.
Core original sin cambios: 80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec.

| Transición | Display | Render a ratio 1.7 | Resultado |
|---|---|---|---|
| Resize: 854×480 → ventana maximizada | 1920×1009 | 1129×593 | PASS |
| Ventana → fullscreen | 1920×1080 | 1129×635 | PASS |
| Fullscreen → ventana | 1920×1009 | 1129×593 | PASS |

En cada transición: escena completa, HUD/crosshair centrados, sin zonas negras ni
clipping de composición observado, sin crash y sin nuevos mensajes GL de error.
Iris conserva enableShaders=true y Complementary-SuperResolution-Wisteria.zip.
SR conserva enable_upscale=true y upscale_ratio=1.7. Los logs muestran dispatch,
inputs de tamaño render, output de tamaño display, viewport completo y FBO COMPLETE.
No representa un stress test ni una auditoría completa de contenido del shader.

Evidencia por transición: logs/runtime/phase-a-resize.{jpg,log},
phase-a-fullscreen.{jpg,log}, phase-a-return-windowed.{jpg,log}.
Resultado recuperable: logs/runtime/phase-a-transition-progress.json.

La corrección previa restauró la comprobación de dimensiones en
ShaderCompatTextureInfo.canUseSourceTextureDirectly: la región -1 se copia a un
input de render-size cuando la textura contenedora es de display-size. FSR1
normaliza usando render-size; recibir el contenedor completo causaba la escena
parcial. El control previo ratio1.0 confirmó RENDER_SIZE_DEPENDENCY_CONFIRMED.
composite7 sí existe/compila/ejecuta y su hook BEFORE sí despacha SR; se descarta
como causa demostrada la hipótesis anterior de pass ausente.

Error GL anterior: glTextureStorage2D, formato sin tamaño GL_DEPTH_COMPONENT
(0x1902), creación interna depthtex 502×282 en ShaderCompatTextureInfo.updateTexture,
vía GlTexture2D.allocateTextureStorage. El resolver ahora consulta precisión/tipo
reales: profundidad 24-bit unsigned normalized; reserva GL_DEPTH_COMPONENT24
(0x81a6). RGBA16F no era la causa. Logs/capturas previos se conservan.

Siguiente fase autorizada: inicialización Vulkan en runtime-baseline con la copia
OpenGL preservada; luego interop y entradas FG si los gates pasan. AMD dispatch y
generated frames = NO. .minecraft real intacto; sin DH/SSRD, push ni publicación.
"""
names = ["COMPLEMENTARY_OPENGL_TEST", "RUNTIME_BASELINE", "CURRENT_STATE", "BASELINE",
         "FRAME_GENERATION_ARCHITECTURE", "FSR_PROVIDER_CONTRACT", "AMD_FSR_FG_PLAN",
         "THIRD_PARTY_ORIGINS", "DEVELOPMENT_LOG"]
for name in names:
    path = root / "docs" / (name + ".md")
    previous = path.read_text(encoding="utf-8")
    if heading in previous:
        continue
    block = "\n\n" + heading + "\n" + detail
    if name == "CURRENT_STATE":
        block = heading + "\n" + detail + "\n\n" + previous
        path.write_text(block, encoding="utf-8")
    else:
        path.write_text(previous + block, encoding="utf-8")
print("Documented closed phase A in nine documents; history retained.")
