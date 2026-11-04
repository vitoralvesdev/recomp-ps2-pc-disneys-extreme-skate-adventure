from pathlib import Path
import struct, csv, re, json
root=Path(__file__).resolve().parents[1]
elf=(root/'original/extracted/SLUS_206.07').read_bytes()
assert elf[:6]==b'\x7fELF\x01\x01'
phoff=struct.unpack_from('<I',elf,28)[0]; phsize,phnum=struct.unpack_from('<HH',elf,42)
def read_words(address,count):
 for i in range(phnum):
  kind,offset,base,_,filesz,_,_,_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if kind==1 and base<=address and address+count*4<=base+filesz:
   return struct.unpack_from('<'+'I'*count,elf,offset+address-base)
 raise ValueError(hex(address))
rows=list(csv.DictReader((root/'analysis/functions.csv').open(encoding='utf-8-sig')))
records=[]
extra = {int(value,16) for value in json.loads((root/'recomp/extra-entry-points.json').read_text(encoding='utf-8-sig'))}
for target in sorted(set(read_words(0x30b2e0,13)) | extra):
 owners=[row for row in rows if int(row['Start'],16)<=target<int(row['End'],16)]
 if not owners:
  recovered=json.loads((root/'analysis/disjoint-initial-functions.json').read_text(encoding='utf-8'))
  assert any(int(item['start'],16)==target for item in recovered), hex(target)
  continue
 owner=min(owners,key=lambda row:int(row['End'],16)-int(row['Start'],16))
 candidates=list((root/'src/generated-ghidra').glob(f'*_0x{int(owner["Start"],16):x}.cpp'))
 assert len(candidates)==1, owner
 source=candidates[0]; text=source.read_text(encoding='utf-8-sig')
 symbol=re.search(r'void (\w+)\(uint8_t\*',text).group(1)
 if target != int(owner['Start'],16):
  label=f'label_{target:x}'
  if label+':' not in text:
   marker=f'    // 0x{target:x}:'
   assert marker in text, (hex(target),source.name)
   text=text.replace(marker,label+':\n'+marker,1)
  if not re.search(r'case 0x'+f'{target:x}'+r'u:',text,re.I):
   text=text.replace('    switch (ctx->pc) {',f'    switch (ctx->pc) {{\n        case 0x{target:x}u: goto {label};',1)
  if source.read_text(encoding='utf-8-sig') != text: source.write_text(text,encoding='utf-8')
 records.append({'target':hex(target),'owner':symbol,'source':source.name})
text='#include "ps2_runtime.h"\n#include "ps2_recompiled_functions.h"\nvoid registerDisneyGuestEntries(PS2Runtime& runtime) {\n'
for record in records:
 text+=f' if (!runtime.hasFunction({record["target"]}u)) runtime.registerFunction({record["target"]}u, &{record["owner"]});\n'
text+='}\n'
(root/'src/host/guest_entries.cpp').write_text(text,encoding='utf-8')
(root/'analysis/recovered-switch-targets.json').write_text(json.dumps({'table':'0x30b2e0','count':13,'source':'original ELF jump table at JR 0x1337b8','entries':records},indent=2),encoding='utf-8')
print(f'{len(records)} switch targets recovered from original ELF')


