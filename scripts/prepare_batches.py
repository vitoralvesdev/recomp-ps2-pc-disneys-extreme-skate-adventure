from pathlib import Path
import argparse
import json
import os

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--generated-dir', type=Path, default=root / 'src/generated-ghidra')
parser.add_argument('--bridges', type=Path, default=root / 'recomp/host-bridges.json')
parser.add_argument('--output', type=Path, default=root / 'build/guest-batches')
args = parser.parse_args()
sources = sorted(args.generated_dir.glob('*.cpp'))
bridges = set(json.loads(args.bridges.read_text(encoding='utf-8'))['excludedSources'])
args.output.mkdir(parents=True, exist_ok=True)
for index in range(0, len(sources), 128):
    group = sources[index:index + 128]
    includes = []
    for item in group:
        if item.name in bridges:
            continue
        relative = Path(os.path.relpath(item, args.output)).as_posix()
        includes.append(f'#include "{relative}"\n')
    destination = args.output / f'guest_{index // 128:04}.cpp'
    text = ''.join(includes)
    if not destination.exists() or destination.read_text(encoding='utf-8-sig') != text:
        destination.write_text(text, encoding='utf-8')
print(f'{len(sources)} sources grouped into {(len(sources) + 127) // 128} batches; {len(bridges)} host bridges')
