# Diagnóstico LWJGL - 2026-10-02

## Causa y detección

El fallo no era una caché corrupta ni un repositorio desconocido. Defender detectó `Trojan:Win32/Vigorf.A` (2147714384, severidad Grave) en `C:/Users/micha/.gradle/.tmp/gradle_download15245544113965602401bin`.

Eventos 1116/1117: detección 2026-10-02 14:39:49 y cuarentena 14:39:52 en la hora mostrada por el host. Los UTC exactos están en logs/baseline/lwjgl-defender-events.json. Proceso: `D:/Programs File2/Eclipse Adoptium/jdk-21.0.12.101-hotspot/bin/java.exe`; remediación Cuarentena, error 0x00000000. Versión firmas 1.459.518.0, motor 1.1.26080.3. La coincidencia exacta con la ruta del error Gradle permite asociar la descarga a ese evento. No demuestra que sea falso positivo. Get-MpThreatDetection dio acceso denegado; los eventos de Defender sí se pudieron leer.

## Tarea, configuración y cadena

SR attempt5: `:common:createMinecraftArtifacts` y `:neoforge:createMinecraftArtifacts`, propiedad `artifactManifestEntries`, configuración `neoFormRuntimeDependenciesRuntimeClasspath` en ambos módulos.

Cadena confirmada con dependencyInsight offline y metadata oficial en caché:

`NeoForm 1.21.1-20240808.144430 -> net.neoforged:minecraft-dependencies:1.21.1 -> clientWindowsNatives -> org.lwjgl:lwjgl:3.3.3:natives-windows-x86 -> force 3.3.4`.

build.gradle.kts de SR fuerza LWJGL en configurations.configureEach, incluidas las configuraciones internas NFRT. configs/1.21.1.json selecciona lwjgl_version 3.3.4 y lwjgl_stb_version 3.3.3. buildSrc carga esos valores; no declara el classifier x86. El classifier viene de la metadata minecraft-dependencies, generada a partir de las librerías de Mojang.

ModDevGradle 2.0.147 conecta esas configuraciones al manifiesto de createMinecraftArtifacts. Su plugin MinecraftDependencies configura selección por sistema operativo/distribución; no tiene atributo de arquitectura. La variante Windows incluye x64, ARM64 y x86, incluso usando JVM amd64.

## Necesidad de x86

- Compilación Java del mod en Windows x64: no necesita DLL de 32 bits. Las clases están en el JAR sin classifier.
- Ejecución x64: usa natives-windows; DLL del JAR 3.3.4 existente comprobada con cabecera PE machine 0x8664 y ruta windows/x64/org/lwjgl/lwjgl.dll.
- Empaquetado del mod: natives-windows-x86 no está declarado en jarJar. SR embebe lwjgl-vulkan y VMA con natives Windows/Linux; esa configuración se conserva.
- El archivo bloqueado se resuelve por recopilación de dependencias runtime de todas las arquitecturas Windows, no por un requisito funcional del mod x64.

## Origen y caché

URL del error: https://repo.maven.apache.org/maven2/org/lwjgl/lwjgl/3.3.4/lwjgl-3.3.4-natives-windows-x86.jar

Maven Central está configurado antes de los demás repositorios en allprojects. Libraries de Mojang también está configurado. No se descargó de un mirror alternativo ni se sustituyó por un fichero manual.

En `C:/Users/micha/.gradle/caches/modules-2/files-2.1/org.lwjgl/lwjgl/3.3.4` se encontraron JAR principal, POM, native x64 y ARM64; ninguno x86 completo/parcial. El temporal exacto ya no existe (cuarentena confirmada). No se borró ninguna entrada de caché. --refresh-dependencies no era necesario: no había metadata obsoleta ni fichero corrupto que reparar; reintentar la descarga bloqueada no solucionaría la causa.

## Acción explicada antes de modificar Gradle

Perfil opcional `-Psr_native_arch=windows-x64`. Regla de metadata pública Gradle, solo en `net.neoforged:minecraft-dependencies/clientWindowsNatives`: elimina dependencias org.lwjgl cuyos artifactSelectors son natives-windows-x86 o natives-windows-arm64. Conserva clases Java, natives-windows, versiones, otros módulos, otras variantes y empaquetado. Guardas: JVM Windows x64, Minecraft 1.21.1 y loader neoforge. Sin propiedad, selección upstream intacta. Commit local e365f5b3.

No existe un selector de arquitectura equivalente en la API actual de MDG; esta es una adaptación local explícita, no una baseline upstream sin cambios. El objetivo es verificar el mismo código Java y gráfico bajo un perfil de dependencias compatible con el host.

Nota: NFRT prepara también Minecraft desde su manifest original 3.3.3 y puede descargar natives originales de otras arquitecturas desde libraries.minecraft.net como fallback cuando no están en su artifact manifest. Eso no cambia el force 3.3.4 de SR ni significa que el pipeline NFRT completo resuelva exclusivamente x64. Se documenta esta limitación y no se reescribe la metadata de Mojang/NFRT.

No se cambió la configuración de Defender, ni se crearon exclusiones o restauraciones desde cuarentena. Get-MpComputerStatus también dio acceso denegado; no se declara una nueva comprobación independiente del estado activo. No cambios a Minecraft, Wisteria, versión LWJGL ni código FG.

## Fuentes primarias complementarias

- [ModDevArtifactsWorkflow](https://github.com/neoforged/ModDevGradle/blob/main/src/main/java/net/neoforged/moddevgradle/internal/ModDevArtifactsWorkflow.java)
- [MinecraftDependenciesPlugin](https://github.com/neoforged/ModDevGradle/blob/main/src/main/java/net/neoforged/minecraftdependencies/MinecraftDependenciesPlugin.java)
- [Generación metadata Minecraft](https://github.com/neoforged/GradleMinecraftDependencies)
- [API reglas metadata Gradle](https://docs.gradle.org/current/userguide/component_metadata_rules.html)

Las fuentes main sirven como explicación; para la versión instalada se contrastó el JAR de MDG 2.0.147 y la metadata exacta en caché. Evidencias: lwjgl-dependency-insight.log, lwjgl-defender-events.json, SR attempt5/attempt6.

## Resultado

SR attempt6 BUILD SUCCESSFUL en 4m 21s. lwjgl-x64-artifact-audit.log confirma LWJGL_AUDIT_OK common/neoforge con base 3.3.4 y natives-windows 3.3.4, sin x86/arm64 en la configuración afectada. No fue necesario descargar de nuevo el artefacto bloqueado. Ambos baseline JARs están verificados en builds/baseline. No es necesario que el usuario cree una exclusión de seguridad para este perfil x64.
