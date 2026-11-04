from pathlib import Path
import csv,re,json,bisect
root=Path(__file__).resolve().parents[1]
rows=list(csv.DictReader((root/'analysis/functions.csv').open(encoding='utf-8-sig')))
ranges=sorted((int(r['Start'],16),int(r['End'],16)) for r in rows)
merged=[]
for start,end in ranges:
 if merged and start<=merged[-1][1]: merged[-1]=(merged[-1][0],max(end,merged[-1][1]))
 else: merged.append((start,end))
starts=[v[0] for v in merged]
found=[]
for source in (root/'src/generated-initial').glob('*.cpp'):
 match_start=re.search(r'_0x([0-9a-fA-F]+)[.]cpp$',source.name)
 if not match_start: continue
 candidate_start=int(match_start.group(1),16)
 candidate_pos=bisect.bisect_right(starts,candidate_start)
 if candidate_pos and candidate_start<merged[candidate_pos-1][1]: continue
 text=source.read_text(encoding='utf-8-sig')
 match=re.search(r'// Address: 0x([0-9a-fA-F]+) - 0x([0-9a-fA-F]+)',text)
 if not match: continue
 start,end=(int(v,16) for v in match.groups())
 pos=bisect.bisect_left(starts,end)
 overlap=pos>0 and merged[pos-1][1]>start
 if not overlap: found.append({'file':source.name,'start':hex(start),'end':hex(end),'bytes':source.stat().st_size})
print(json.dumps({'count':len(found),'bytes':sum(item['bytes'] for item in found),'first':found[:8]},indent=2))
(root/'analysis/disjoint-initial-functions.json').write_text(json.dumps(found,indent=2),encoding='utf-8')

