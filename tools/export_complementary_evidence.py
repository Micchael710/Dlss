"""Snapshot this isolated shader test's logs without changing game configuration."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
logs = root / "logs/runtime"
def sanitize(text):
    text = text.replace("C:\\Users\\micha", "<USER_HOME>").replace("C:/Users/micha", "<USER_HOME>")
    text = text.replace("lwjgl_micha", "lwjgl_TEST_USER")
    return re.sub(r"(--accessToken\s+)\S+", r"\1<REDACTED>", text)
latest = sanitize((root / "runtime-baseline/instance/logs/latest.log").read_text(encoding="utf-8"))
(logs / "complementary-opengl-game.log").write_text(latest, encoding="utf-8")
lines = latest.splitlines()
selected = []
take_stack = False
for number, line in enumerate(lines, 1):
    if line.startswith("["):
        take_stack = bool(re.search(r"/ERROR\]|/FATAL\]|type=ERROR|Failed to resolve uniform|uniforms won't work|block ID map entry|has no property", line))
    if take_stack:
        selected.append(f"latest.log:{number}: {line}")
(logs / "complementary-opengl-errors.log").write_text("\n".join(selected) + "\n", encoding="utf-8")
client = logs / "complementary-opengl-client.log"
(logs / "complementary-opengl-client-sanitized-snapshot.log").write_text(sanitize(client.read_text(encoding="utf-8")), encoding="utf-8")
summary = {"test": "Complementary OpenGL", "status": "FAILED_VISUAL_GATE",
           "pack_selected": True, "pack_active": True, "world_loaded": True,
           "config_processed": "superresolution.v1.json (preferred over unversioned config)",
           "binding_resolution_and_dispatch": "NOT_INDIVIDUALLY_VERIFIED_RUNTIME",
           "visual_failure": "scene restricted to lower-left render-sized region; black top/right; HUD spans display",
           "opengl_error": "1280 GL_INVALID_ENUM Internal format not supported (source call not identified)",
           "resize": "NOT_RUN", "fullscreen": "NOT_RUN", "vulkan": "NOT_RUN",
           "generated_frames": False, "visually_validated": False,
           "source_diagnostic_candidate": "composite7 conditional enablement versus BEFORE composite7 trigger; unproven runtime causality",
           "crash": "NONE_OBSERVED_AT_SNAPSHOT", "last_log_record": lines[-1]}
(logs / "complementary-opengl-result.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
print(json.dumps(summary, indent=2))
