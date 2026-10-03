# Migración a repositorio del usuario — 2026-10-02

## Actualización vigente tras x2

SR y Wisteria quedaron sin remotes Git tras obtener y validar la ruta AMD requerida,
de acuerdo con la petición previa del usuario de desenlazar los repositorios ajenos.
Se conservaron los commits locales, licencias, URLs/SHA de procedencia y las refs
anteriores en logs/runtime/{sr,wisteria}-before-upstream-detach-refs.txt. No push/PR.
No se eliminó ni reescribió el historial. URL del repositorio propio aún pendiente.
Esto no afirma que estén cerrados todos los requisitos de distribución/licencias.
Las tablas y el procedimiento siguientes son el plan histórico anterior al detach;
al recibir URL propia no habrá que renombrar un origin upstream, porque ya no existe.

Estado: **NOT_READY_FOR_OWN_REPOSITORY**. No migración ejecutada; URL del usuario todavía no suministrada. Upstream se conserva solamente como lectura y procedencia.

| Condición solicitada | Evidencia / estado |
|---|---|
| Arquitectura entendida | Mapa estático terminado; runtime y native safety pendientes |
| Comparación dirigida | Terminada con límites de evidencia en REFERENCE_DIFF.md |
| SDK elegido | 1.1.4 / FSR3.1.4 / Vulkan candidato documentado; package/pin DLL/direct path pendientes |
| Código y submódulos locales completos | NO: cinco gitlinks SR sin fuentes locales; SDK FG no adquirido |
| Licencias documentadas y completas | Inventario creado, cierre NO: Wisteria LICENSE y notices/versiones transitivas/package pendientes |
| Proyecto construible localmente | Ambos Java JARs COMPILED; no demuestra rebuild nativo desde fuente ni independencia de descargas prebuilt |
| Preparado sin checkout remoto | NO demostrado mediante fuente/dependency cache completa y build reproducible |

La baseline no depende de escribir en el GitHub upstream. Falta independencia para reconstruir todos los componentes necesarios. No confundir esta condición con disponibilidad de Maven Central: nuestro proyecto podrá tener dependencias oficiales remotas fijadas, pero necesita source/pin/provenance y un proceso definido sin checkout upstream mutable.

## Protección actual comprobada

SR origin fetch https://github.com/IReallyWantToSleep/superresolution.git; Wisteria origin fetch https://github.com/IReallyWantToSleep/Wisteria.git. En ambos origin push `DISABLED_UPSTREAM_PUSH`. Árboles clean y HEADs sin cambio respecto de baseline. No push/PR/fork publicado/rama remota. Conservación temporal del nombre origin según petición actual de mantener investigación read-only; no es destino de nuestro desarrollo remoto.

Un push URL inválido protege el push ordinario al remote, no impide a un operador escribir a una URL explícita. La regla del usuario sigue siendo no hacer ninguna escritura upstream. No editar credenciales, permisos globales ni Git safe.directory.

## Procedimiento futuro cuando condiciones y URL estén disponibles

1. Inventariar repos Git separados y decidir estructura del repositorio propio (monorepo con historial importado o repos separados); no inventar URLs ni borrar los .git. Conservar SHA base/commits locales y export de provenance/submodules/licenses; incluir docs de raíz, hoy fuera de ambos repos Git.
2. Completar fuentes/submódulos necesarios en commits fijados y registrar fuentes nativas/prebuilt autorizadas. Mantener .gitmodules con procedencia legítima; no eliminar el origen de terceros para simular código original.
3. Cuando autorizado por la URL del usuario, renombrar origin actual a upstream en cada checkout pertinente; preservar fetch, reemplazar/eliminar cualquier push URL upstream legítima y mantener sentinel disabled. Verificar fetch URL/push URL antes de cada acción. Alternativa: retirar remote upstream únicamente tras guardar provenance y si ya no ayuda a comparar.
4. Configurar origin nuevo con la URL EXACTA facilitada por el usuario, respetando la estructura elegida; no convertir por accidente ambos proyectos en el mismo repositorio sin importar historia deliberadamente. No reutilizar una URL del autor como origin de publicación.
5. Revisar lista exacta a publicar: source/build scripts/docs/licenses, sin caches, tokens, .minecraft, JARs de referencia, DLLs prohibidas ni binarios sin notices. Confirmar los remotes y el historial resultante.
6. Hacer push al repositorio propio solamente cuando el usuario lo autorice; proporcionar URL no obliga por sí solo a publicar todo. Nunca push a upstream ni PR al autor.

No se marcará READY_FOR_OWN_REPOSITORY hasta cumplir todas las condiciones de la tabla. No hay necesidad de desconectar lectura ahora para mantener el trabajo local.
