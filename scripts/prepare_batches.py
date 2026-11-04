from pathlib import Path
import json
root = Path(__file__).resolve().parents[1]
sources = sorted((root / 'src/generated-ghidra').glob('*.cpp'))
bridges = set(json.loads((root / 'recomp/host-bridges.json').read_text(encoding='utf-8'))['excludedSources'])
output = root / 'build/guest-batches'
output.mkdir(parents=True, exist_ok=True)
for index in range(0, len(sources), 128):
    group = sources[index:index+128]
    text = ''.join('#include "../../src/generated-ghidra/' + item.name + '"\n' for item in group if item.name not in bridges)
    destination = output / f'guest_{index//128:04}.cpp'
    if not destination.exists() or destination.read_text(encoding='utf-8-sig') != text:
        destination.write_text(text, encoding='utf-8')
print(f'{len(sources)} sources grouped into {(len(sources)+127)//128} batches; {len(bridges)} host bridges')


