from pathlib import Path
import argparse
import csv
import json
import re
import struct

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--elf', type=Path, default=root / 'original/extracted/SLUS_206.07')
parser.add_argument('--functions-csv', type=Path, default=root / 'analysis/functions.csv')
parser.add_argument('--generated-dir', type=Path, default=root / 'src/generated-ghidra')
parser.add_argument('--extra-entry-points', type=Path, default=root / 'recomp/extra-entry-points.json')
parser.add_argument('--disjoint-functions', type=Path, default=root / 'analysis/disjoint-initial-functions.json')
parser.add_argument('--guest-output', type=Path, default=root / 'src/host/guest_entries.cpp')
parser.add_argument('--report', type=Path, default=root / 'analysis/recovered-switch-targets.json')
args = parser.parse_args()
elf = args.elf.read_bytes()
assert elf[:6] == b'\x7fELF\x01\x01'
phoff = struct.unpack_from('<I', elf, 28)[0]
phsize, phnum = struct.unpack_from('<HH', elf, 42)


def read_words(address, count):
    for i in range(phnum):
        kind, offset, base, _, filesz, _, _, _ = struct.unpack_from('<8I', elf, phoff + i * phsize)
        if kind == 1 and base <= address and address + count * 4 <= base + filesz:
            return struct.unpack_from('<' + 'I' * count, elf, offset + address - base)
    raise ValueError(hex(address))


rows = list(csv.DictReader(args.functions_csv.open(encoding='utf-8-sig')))
records = []
extra = {int(value, 16) for value in json.loads(args.extra_entry_points.read_text(encoding='utf-8-sig'))}
recovered = json.loads(args.disjoint_functions.read_text(encoding='utf-8'))
for target in sorted(set(read_words(0x30b2e0, 13)) | extra):
    owners = [row for row in rows if int(row['Start'], 16) <= target < int(row['End'], 16)]
    if not owners:
        assert any(int(item['start'], 16) == target for item in recovered), hex(target)
        continue
    owner = min(owners, key=lambda row: int(row['End'], 16) - int(row['Start'], 16))
    candidates = list(args.generated_dir.glob(f'*_0x{int(owner["Start"], 16):x}.cpp'))
    assert len(candidates) == 1, owner
    source = candidates[0]
    text = source.read_text(encoding='utf-8-sig')
    symbol = re.search(r'void (\w+)\(uint8_t\*', text).group(1)
    if target != int(owner['Start'], 16):
        label = f'label_{target:x}'
        if label + ':' not in text:
            marker = f'    // 0x{target:x}:'
            assert marker in text, (hex(target), source.name)
            text = text.replace(marker, label + ':\n' + marker, 1)
        if not re.search(r'case 0x' + f'{target:x}' + r'u:', text, re.I):
            text = text.replace('    switch (ctx->pc) {',
                                f'    switch (ctx->pc) {{\n        case 0x{target:x}u: goto {label};', 1)
        if source.read_text(encoding='utf-8-sig') != text:
            source.write_text(text, encoding='utf-8')
    records.append({'target': hex(target), 'owner': symbol, 'source': source.name})
text = '#include "ps2_runtime.h"\n#include "ps2_recompiled_functions.h"\nvoid registerDisneyGuestEntries(PS2Runtime& runtime) {\n'
for record in records:
    text += f' if (!runtime.hasFunction({record["target"]}u)) runtime.registerFunction({record["target"]}u, &{record["owner"]});\n'
text += '}\n'
args.guest_output.parent.mkdir(parents=True, exist_ok=True)
args.guest_output.write_text(text, encoding='utf-8')
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps({'table': '0x30b2e0', 'count': 13, 'source': 'original ELF jump table at JR 0x1337b8', 'entries': records}, indent=2), encoding='utf-8')
print(f'{len(records)} switch targets recovered from original ELF')
