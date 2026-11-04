from pathlib import Path
import json,hashlib,wave,shutil,math,audioop,array,sys
p=Path(sys.argv[1]).resolve();w=Path(sys.argv[2]).resolve();dest=p/'assets/native/audio'
old=json.loads((p/'analysis/audio-asset-installation.json').read_text(encoding='utf-8'));missing=set(old['missingMusic'])
rows=[r.split('\t') for r in (dest/'catalog.tsv').read_text(encoding='utf-8').splitlines()]
keys={a[0] for a in rows};originals={a[0]:p/a[1] for a in rows};stats=[]
if not missing and old.get('musicRecovery',{}).get('newTracks')==27:
    recovered=[a for a in old['sounds'] if a.get('source',{}).get('kind')=='PS2 ISO stereo ADPCM converted to PCM']
    assert len(recovered)==27 and len([key for key in keys if key.startswith('music/')])==37
    for a in recovered:assert hashlib.sha256((dest/a['file']).read_bytes()).hexdigest()==a['sha256']
    print('37 music entries already installed; 27 recovered PCM hashes verified.');raise SystemExit(0)

def segment(path,start,count,channel):
 with wave.open(str(path),'rb') as f:
  start=max(0,min(start,f.getnframes()-count));f.setpos(start);raw=f.readframes(count)
 return audioop.tomono(raw,2,1 if channel==0 else 0,1 if channel==1 else 0)
def correlation(a,b):
 av=array.array('h',a);bv=array.array('h',b);xy=sum(x*y for x,y in zip(av,bv));xx=sum(x*x for x in av);yy=sum(y*y for y in bv);return xy/math.sqrt(xx*yy) if xx*yy else None
for line in (w/'decoded.tsv').read_text(encoding='utf-8-sig').splitlines():
 name,offset,size,frames,rate,clips=line.split('\t');frames=int(frames);file=w/'pcm'/f'{name}.wav'
 with wave.open(str(file),'rb') as f:
  assert f.getnchannels()==2 and f.getsampwidth()==2 and f.getframerate()==48000 and f.getnframes()==frames
  assert len(f.readframes(frames))==frames*4
 key='music/'+name
 rec={'key':key,'offset':int(offset),'logicalCompressedBytes':int(size),'physicalReadBytes':math.ceil(int(size)/0x30000)*0x30000,'frames':frames,'rate':48000,'channels':2,'decoderClippingCount':int(clips),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'durationSeconds':frames/48000}
 if key in originals:
  comparisons=[]
  for at in [48000,60*48000,frames-48000]:
   for ch in [0,1]:
    a=segment(file,at,1024,ch);b=segment(originals[key],at-2048,8192,ch);off,gain=audioop.findfit(b,a);rho=correlation(a,b[off*2:(off+1024)*2])
    comparisons.append({'frame':at,'channel':ch,'offset':off-2048,'correlation':rho})
    if rho is not None:assert rho>.995,(key,at,ch,rho)
  rec['referenceComparison']=comparisons
 stats.append(rec)
assert len(stats)==37 and {s['key'] for s in stats if s['key'] in missing}==missing and len(missing)==27
backups=w/'before';backups.mkdir(exist_ok=True)
for source in [dest/'catalog.tsv',p/'analysis/audio-asset-installation.json']:
 target=backups/source.name
 if not target.exists():shutil.copyfile(source,target)
for rec in stats:
 if rec['key'] not in missing:continue
 name=rec['key'].split('/')[-1];filename='files/'+rec['sha256']+'.wav';target=dest/filename
 if target.exists():assert hashlib.sha256(target.read_bytes()).hexdigest()==rec['sha256']
 else:shutil.copyfile(w/'pcm'/f'{name}.wav',target)
 rows.append([rec['key'],'assets/native/audio/'+filename,'0','0'])
 old['sounds'].append({'key':rec['key'],'original':f"MUSIC.WAD@{rec['offset']}:{rec['logicalCompressedBytes']}",'file':filename,'sha256':rec['sha256'],'rate':48000,'frames':rec['frames'],'loopStart':0,'loopEnd':0,'source':{'kind':'PS2 ISO stereo ADPCM converted to PCM','interleaveBytes':0x18000,'physicalPaddingRead':True}})
assert len(rows)==len({a[0] for a in rows})
old['installedLogicalSounds']=len(rows);old['missingMusic']=[]
old['limitations']=[x for x in old.get('limitations',[]) if 'Music soundtrack tracks absent' not in x]
old['musicRecovery']={'date':'2026-10-25','newTracks':27,'totalMusicKeys':37,'source':'Local original/extracted/MUSIC/MUSIC.WAD','youtubePlaylistReference':'https://www.youtube.com/watch?v=ygPl-t01_U0&list=PLxLkpANYnG3_19LskNpZQEmsh7cn83Bwz','downloadedFromYouTube':False,'validation':'37 RIFF PCM files; 10 ambience tracks compared with existing GameCube references; native mixer tests recorded separately.'}
temporary=dest/'catalog.tsv.music-recovery.tmp';temporary.write_text(''.join('\t'.join(r)+'\n' for r in rows),encoding='utf-8');temporary.replace(dest/'catalog.tsv')
(p/'analysis/audio-asset-installation.json').write_text(json.dumps(old,ensure_ascii=False,indent=2),encoding='utf-8')
report={'newTracks':27,'totalMusicKeys':37,'catalogEntries':len(rows),'tracks':stats,'source':old['musicRecovery'],'backup':str(backups)}
(w/'music-installation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
(p/'analysis/music-recovery-2026-10-25.json').write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
print(json.dumps({k:report[k] for k in ['newTracks','totalMusicKeys','catalogEntries']},indent=2))
