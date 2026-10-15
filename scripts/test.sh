#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

./build_linux.sh
./scripts/download_test_images.sh

if [[ ! -d models ]]; then
  echo "[!] models/ missing — run ./scripts/download_models.sh first" >&2
  exit 1
fi

SBS_PREFIX="${SBS_PREFIX:-$HOME/sbs/_/x86_64-linux-gnu}"
export LD_LIBRARY_PATH="$PWD/dist:$SBS_PREFIX/lib:${LD_LIBRARY_PATH:-}"

OUT="/tmp/eyes-test-out.json"
DATA="$(mktemp -d)"
FIRST=""
trap 'rm -rf "$DATA"; [[ -n "${FIRST:-}" ]] && rm -f "$FIRST"' EXIT
for img in testdata/images/*; do
  [[ -f "$img" ]] || continue
  echo "[+] analyzing $img"
  EYES_ASSETS_DIR="$PWD/models" "${EYES_BIN:-./dist/test_eyes}" "$img" "$OUT" "$DATA"
  if [[ "$(basename "$img")" == "faces.jpg" ]]; then
    FIRST="$(mktemp)"
    cp "$OUT" "$FIRST"
  fi
  python3 - <<PY
import json,sys
j=json.load(open("$OUT"))
print("  objects:", len(j.get("objects",[])))
print("  ocr:", len(j.get("ocr",[])))
print("  codes:", len(j.get("codes",[])))
print("  faces:", len(j.get("faces",[])))
print("  people:", sorted({f.get("personId") for f in j.get("faces",[]) if f.get("personId")}))
print("  caption:", (j.get("description",{}).get("caption") or "")[:80])
print("  tags:", [t["label"] for t in j.get("description",{}).get("tags",[])[:3]])
print("  phash:", j.get("extras",{}).get("phash"))
PY
done

if [[ -n "$FIRST" ]]; then
  echo "[+] re-analyzing faces.jpg against the same gallery"
  EYES_ASSETS_DIR="$PWD/models" "${EYES_BIN:-./dist/test_eyes}" testdata/images/faces.jpg "$OUT" "$DATA"
  python3 - "$FIRST" "$OUT" "$DATA" <<'PY'
import hashlib, json, os, sys
first = json.load(open(sys.argv[1]))
second = json.load(open(sys.argv[2]))
a = [f.get("personId") for f in first.get("faces", [])]
b = [f.get("personId") for f in second.get("faces", [])]
if a != b or len({x for x in a if x}) < 2:
    raise SystemExit(f"face identities changed or did not split people: {a} vs {b}")
face_dir = os.path.join(sys.argv[3], "faces")
n = 0
for name in os.listdir(face_dir):
    if not name.endswith(".bin"):
        continue
    digest = hashlib.sha256(open(os.path.join(face_dir, name), "rb").read()).hexdigest()
    if digest + ".bin" != name:
        raise SystemExit(f"gallery file {name} is not the sha256 of its bytes")
    n += 1
if n < 2:
    raise SystemExit(f"expected stored faces, found {n}")
print(f"  stable people: {len(set(a))} across {n} gallery files")
PY
fi

echo "[+] classifying checked-in people"
PEOPLE_DATA="$(mktemp -d)"
PEOPLE_OUT="$(mktemp -d)"
trap 'rm -rf "$DATA" "$PEOPLE_DATA" "$PEOPLE_OUT"; [[ -n "${FIRST:-}" ]] && rm -f "$FIRST"' EXIT
for img in testdata/people/*.jpg; do
  [[ -f "$img" ]] || continue
  name="$(basename "$img")"
  echo "[+] $name"
  EYES_ASSETS_DIR="$PWD/models" "${EYES_BIN:-./dist/test_eyes}" "$img" "$PEOPLE_OUT/$name.json" "$PEOPLE_DATA"
done
python3 - "$PEOPLE_OUT" <<'PY'
import json, sys
from pathlib import Path

out = Path(sys.argv[1])

def load(name):
    return json.loads((out / f"{name}.json").read_text())

def person_ids(doc):
    return [f.get("personId") for f in doc.get("faces", []) if f.get("personId")]

def person_objects(doc):
    return [o for o in doc.get("objects", []) if o.get("label") == "person"]

groups = {
    "albert-einstein.jpg": "einstein",
    "albert-einstein-again.jpg": "einstein",
    "abraham-lincoln.jpg": "lincoln",
    "marie-curie.jpg": "curie",
}
seen = {}
for name, group in groups.items():
    doc = load(name)
    ids = person_ids(doc)
    if len(doc.get("faces", [])) != 1 or len(ids) != 1:
        raise SystemExit(f"{name}: expected one identified face, got {doc.get('faces')}")
    if len(person_objects(doc)) < 1:
        raise SystemExit(f"{name}: expected a person detection, got {doc.get('objects')}")
    seen.setdefault(group, set()).add(ids[0])
    print(f"  {name}: person {ids[0][:12]}")

if any(len(ids) != 1 for ids in seen.values()):
    raise SystemExit(f"a person was split across ids: {seen}")
if len({next(iter(ids)) for ids in seen.values()}) != len(seen):
    raise SystemExit(f"different people shared an id: {seen}")

empty = load("no-person.jpg")
if empty.get("faces") or person_objects(empty):
    raise SystemExit(f"no-person.jpg was classified as a person: faces={empty.get('faces')} objects={empty.get('objects')}")
print("  no-person.jpg: no person")
PY
echo "[+] all tests passed"
