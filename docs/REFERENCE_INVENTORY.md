# Referencias verificadas - 2026-10-02

Base de rutas: `C:/Users/micha/AppData/Roaming/.minecraft/versions/Apparatia Alpha/`.
Todas se leyeron sin modificar ni copiar. Búsqueda en workspace y attachments no encontró copias anteriores de estos artefactos ni MINECRAFT_RENDER_AUDIT.md.

| Archivo | SHA-256 |
|---|---|
| mods/super_resolution-neoforge-1.21..1.21.1-0.9.1-alpha.2+opengl.jar | 9C1C5A5256F872122AD9BB1A8F315B0A13AB2D4A69072FE2EB6EBDE13950C92B |
| mods/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1.jar | CDC737D74D61BFB88427BF416ADA654C26BDC7C5CCC447DC7B7FFD8FDF9F70EC |
| shaderpacks/Complementary-SuperResolution-Wisteria.zip | 680E76EE11B0513C84602C3A3E151C64C236C5BC952FFF4492265B8D4A4CF7D7 |
| shaderpacks/Complementary-SuperResolution-Wisteria.zip.txt | 6337DFD11AD562FE5B1FD99AFE9F40D2FEF05399B393081355982C6F36BAF56C |

## Inspección inicial, no comparación completa de fase E

Wisteria JAR contiene backend/fsr/FsrFgBackend, FsrFrameGenerationAdapter, FsrFrameGenerationBackend, FsrNativeBridge y natives/WisteriaNative.dll (también .so). La mera presencia de esas clases no demuestra que use el SDK de AMD. La solicitud describe un interpolador Vulkan propio y crashes 0xc0000005; estas afirmaciones requieren inspección dirigida del puente y código nativo antes de clasificarlas como verificadas.

Super Resolution JAR contiene registro/provider FG, execution model, request/result asíncronos, constantes y AsyncFrameGenerationScheduler. Su versión 0.9.1-alpha.2 difiere del checkout 0.9.2-alpha.1; no se debe usar como dependencia de desarrollo ni asumir equivalencia de API.

Shader: superresolution.json raíz y shaders/superresolution.json son iguales. Color autotex3, profundidad depthtex, motion autotex2; upscale BEFORE composite7, rgba16f, HDR, auto exposure, jitter enabled y motion_jittered=false. Se leyeron motionVectors.glsl, composite6.glsl, composite7.glsl, final.glsl y .zip.txt completos.

composite6 escribe color y vectores a DRAWBUFFERS:32 y calcula movimiento usando depthtex1. composite7 y final definen AFTER_SR. motionVectors reconstruye posición usando profundidad y matrices de cámara; no incorpora transformaciones previas independientes de entidades: sus movimientos/animación no quedan completamente representados. Cero para profundidad <=0.56; cielo usa reproyección sin traslación, y existen ramas para DH/VOXY. La compatibilidad de depthtex con depthtex1 necesita verificación del mapeo de captura.

Mantener pendientes: jitter temporal, ghosting, buffers de distinta resolución, cielo/fondo recortado, profundidad DH, nubes y reflejos. Son problemas reportados por el usuario; no se han reproducido visualmente aquí. No corregirlos simultáneamente ni considerar las ramas DH como validación de FG con DH.
