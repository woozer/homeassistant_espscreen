#!/usr/bin/env python3
"""Write the firmware-source fingerprint embedded in the host renderer."""
from hashlib import sha256
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
files = [root / "components/smart_display/runtime_tiles.h", root / "components/smart_display/renderer_host_api.h"]
seen = set()
pending = list(files)
while pending:
    path = pending.pop()
    if path in seen:
        continue
    seen.add(path)
    if not path.is_file():
        raise SystemExit(f"missing firmware renderer source: {path}")
    for name in re.findall(r'^\s*#include\s+"([^"]+)"', path.read_text(errors="replace"), re.M):
        candidate = (path.parent / name).resolve()
        if candidate.is_relative_to(root) and candidate.is_file():
            pending.append(candidate)
files = sorted(seen)
digest = sha256()
for path in files:
    if not path.is_file():
        raise SystemExit(f"missing firmware renderer source: {path}")
    digest.update(str(path.relative_to(root)).encode())
    digest.update(path.read_bytes())
out = root / "web/wasm/generated/firmware_renderer_manifest.h"
out.write_text("// Generated. Do not edit; regenerate with web/wasm/build.sh.\n" +
               f'#define ESP_SCREEN_FIRMWARE_RENDERER_SOURCE_SHA256 "{digest.hexdigest()}"\n')
print(digest.hexdigest())
