from pathlib import Path
import csv,struct,re,bisect,json
root=Path(__file__).resolve().parents[1]; elf=(root/'original/extracted/SLUS_206.07').read_bytes()
rows=list(csv.DictReader((root/'analysis/functions.csv').open(encoding='utf-8-sig')))
ranges=sorted((int(r['Start'],16),int(r['End'],16)) for r in rows); merged=[]
for start,end in ranges:
 if merged and start<=merged[-1][1]: merged[-1]=(merged[-1][0],max(end,merged[-1][1]))
 else: merged.append((start,end))
starts=[x[0] for x in merged]
shoff=struct.unpack_from('<I',elf,32)[0]; shsize,shnum=struct.unpack_from('<HH',elf,46)
sections=[struct.unpack_from('<10I',elf,shoff+i*shsize) for i in range(shnum)]
exec_ranges=[(s[3],s[3]+s[5]) for s in sections if s[2]&4]
refs=set()
for section in sections:
 if section[1]!=1 or not section[2]&2 or section[2]&4: continue
 for offset in range(section[4],section[4]+section[5]-3,4):
  value=struct.unpack_from('<I',elf,offset)[0]
  if value&3 or not any(a<=value<b for a,b in exec_ranges): continue
  pos=bisect.bisect_right(starts,value)
  if not pos or value>=merged[pos-1][1]: refs.add(value)
refs.add(0x23ae48)
files=[]
for file in (root/'src/generated-initial').glob('*.cpp'):
 match=re.search(r'_0x([0-9a-fA-F]+)[.]cpp$',file.name)
 if match: files.append((int(match.group(1),16),file))
files.sort(); initial_starts=[v[0] for v in files]; cache={}; records={}
for target in sorted(refs):
 pos=bisect.bisect_right(initial_starts,target)
 if not pos: continue
 start,source=files[pos-1]
 if source not in cache: cache[source]=source.read_text(encoding='utf-8-sig')
 text=cache[source]
 bounds=re.search(r'// Address: 0x([0-9a-fA-F]+) - 0x([0-9a-fA-F]+)',text)
 if not bounds or target>=int(bounds.group(2),16): continue
 marker=re.search(r'// 0x'+f'{target:x}'+r':[^\n]*',text)
 if not marker or 'Delay Slot' in marker.group(0): continue
 item=records.setdefault(source.name,{'file':source.name,'start':hex(start),'end':hex(int(bounds.group(2),16)),'entryPoints':[]})
 item['entryPoints'].append(hex(target))
(root/'recomp/recovered-overlap-entries.json').write_text(json.dumps(list(records.values()),indent=2),encoding='utf-8')
print(f'{sum(len(v["entryPoints"]) for v in records.values())} ELF data-referenced code entries in {len(records)} original function bodies')
