"""Install exact-name GameCube WAVs and recover PS2 effects/voice streams.
Does not run downloaded content. Only uncompressed PCM WAV audio is accepted.
"""
from pathlib import Path, PurePosixPath
from collections import defaultdict
from array import array
import argparse, hashlib, io, json, struct, wave, zipfile, sys

parser = argparse.ArgumentParser()
parser.add_argument('root', nargs='?', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--game-dir', type=Path)
parser.add_argument('--downloads-dir', type=Path, default=Path.home() / 'Downloads')
args = parser.parse_args()
ROOT = args.root.resolve()
GAME_DIR = (args.game_dir or ROOT / 'original/extracted').resolve()
DOWNLOADS = args.downloads_dir.resolve()
IDS = [446313,446314,446315,446316,446317,446318,446319,446320,446321,446322,446323,446324,446325,446326,446327,446328,446329,446330,454160]
BASE = 'https://sounds.spriters-resource.com/gamecube/disneysextremeskateadventure/'
DEST = ROOT / 'assets/native/audio'
DEST.mkdir(parents=True, exist_ok=True)
sha = lambda b: hashlib.sha256(b).hexdigest()
gc, archives, skipped = {}, [], []
missing_archives = [str(DOWNLOADS / f'{asset_id}.zip') for asset_id in IDS
                    if not (DOWNLOADS / f'{asset_id}.zip').is_file()]
if missing_archives:
    raise FileNotFoundError('Pacotes WAV ausentes em Downloads: ' + ', '.join(missing_archives))
print('[DISNEY_PROGRESS] 5 Verificando os pacotes WAV originais...', flush=True)
for archive_index, asset_id in enumerate(IDS):
    print(f'[DISNEY_PROGRESS] {10 + 40 * archive_index // len(IDS)} Lendo o pacote WAV {archive_index + 1}/{len(IDS)}...', flush=True)
    archive = DOWNLOADS / f'{asset_id}.zip'
    archives.append({'asset':asset_id, 'page':f'{BASE}asset/{asset_id}/', 'sha256':sha(archive.read_bytes())})
    with zipfile.ZipFile(archive) as z:
        bad = z.testzip()
        if bad: raise ValueError(f'ZIP integrity failed: {bad}')
        for info in z.infolist():
            if info.is_dir(): continue
            path = PurePosixPath(info.filename)
            if path.is_absolute() or '..' in path.parts or path.suffix.lower() != '.wav':
                raise ValueError(f'Unexpected archive content: {info.filename}')
            if 'French' in path.parts or 'German' in path.parts:
                skipped.append(info.filename); continue
            data = z.read(info)
            with wave.open(io.BytesIO(data)) as w:
                assert w.getcomptype()=='NONE' and w.getsampwidth()==2 and w.getnchannels() in (1,2)
                rate, frames = w.getframerate(), w.getnframes()
                assert 8000 <= rate <= 192000 and frames > 0
                assert len(w.readframes(frames)) == frames*w.getnchannels()*2
            key = path.stem.lower()
            record={'data':data,'rate':rate,'frames':frames,'archive':asset_id,'member':info.filename}
            if key in gc and gc[key]['data'] != data: raise ValueError(f'Ambiguous English name: {key}')
            gc[key]=record

FILTERS=((0,0),(60,0),(115,-52),(98,-55),(122,-60))
def decode_adpcm(data):
    pcm=array('h'); old=older=0; start=end=None
    for at in range(0,len(data)-15,16):
        b=data[at:at+16]; shift=b[0]&15; filt=b[0]>>4
        if shift>12 or filt>4: raise ValueError(f'Invalid ADPCM at {at}: {b[:2].hex()}')
        flag=b[1]
        if flag & 4: start=len(pcm)
        a,c=FILTERS[filt]
        for v in b[2:]:
            for n in (v&15,v>>4):
                n=n-16 if n>=8 else n
                sample=(n<<12)>>shift
                sample+=(old*a+older*c+32)>>6
                sample=max(-32768,min(32767,sample))
                older,old=old,sample;pcm.append(sample)
        if flag&1:
            if flag&2: end=len(pcm)
            break
    if sys.byteorder!='little': pcm.byteswap()
    return pcm, start, end

def wav_bytes(pcm,rate):
    f=io.BytesIO()
    with wave.open(f,'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(pcm.tobytes())
    return f.getvalue()

rows=[]; records=[]; used=set(); errors=[]
def install(key, original, data, rate, frames, loop_start=0, loop_end=0):
    stem=PurePosixPath(key).name
    hit=gc.get(stem)
    if hit:
        data=hit['data']; newrate=hit['rate']; frames=hit['frames']
        loop_start=round(loop_start*newrate/rate)
        loop_end=min(frames,round(loop_end*newrate/rate))
        rate=newrate; used.add(stem)
        source={'kind':'GameCube WAV','asset':hit['archive'],'member':hit['member']}
    else: source={'kind':'PS2 ISO ADPCM converted to PCM'}
    digest=sha(data); filename=f'files/{digest}.wav'
    target=DEST/filename; target.parent.mkdir(exist_ok=True)
    if not target.exists():target.write_bytes(data)
    if loop_end<=loop_start:loop_start=loop_end=0
    rows.append((key,'assets/native/audio/'+filename,str(loop_start),str(loop_end)))
    records.append({'key':key,'original':original,'file':filename,'sha256':digest,'rate':rate,'frames':frames,'loopStart':loop_start,'loopEnd':loop_end,'source':source})

# Full original resource paths disambiguate names in different sound folders.
print('[DISNEY_PROGRESS] 55 Convertendo efeitos VAG extraidos...', flush=True)
vags=defaultdict(list)
for f in (ROOT/'assets/source/wad').rglob('*.vag'):
    tail=str(f).replace('\\','/').lower().split('/sounds/vag/',1)[1][:-4]
    vags[tail].append(f)
for tail, paths in sorted(vags.items()):
    binaries={sha(f.read_bytes()):f for f in paths}
    if len(binaries)!=1: raise ValueError(f'Different PS2 sounds share logical path: {tail}')
    f=paths[0];b=f.read_bytes();assert b[:4]==b'VAGp'
    length,rate=struct.unpack_from('>II',b,12)
    assert length<=len(b)-48 and rate>0
    pcm,start,end=decode_adpcm(b[48:48+length])
    install('sfx/'+tail,str(f.relative_to(ROOT)),wav_bytes(pcm,rate),rate,len(pcm),start or 0,end or 0)

def hed_entries(path):
    b=path.read_bytes();at=0
    while at+8<=len(b):
        offset,size=struct.unpack_from('<II',b,at)
        if offset==0xffffffff:break
        end=b.find(b'\0',at+8)
        if end<0: raise ValueError('Unterminated HED name')
        name=b[at+8:end].decode('ascii').replace('\\','/').strip('/').lower()
        yield offset,size,name
        at=(end+4)&~3

# EZPCM voice stream samples run at pitch 4096 (48 kHz); unlike music, mono.
print('[DISNEY_PROGRESS] 75 Convertendo vozes e streams do disco...', flush=True)
with (GAME_DIR/'STREAMS/STREAMS.WAD').open('rb') as f:
    for offset,size,name in hed_entries(GAME_DIR/'STREAMS/STREAMS.HED'):
        stem=name.rsplit('/',1)[-1]
        if stem in gc:
            hit=gc[stem];install('stream/'+name.removeprefix('streams/'),f'STREAMS.WAD@{offset}:{size}',hit['data'],hit['rate'],hit['frames'])
        else:
            f.seek(offset);data=f.read(size)
            try:
                pcm,_,_=decode_adpcm(data)
                install('stream/'+name.removeprefix('streams/'),f'STREAMS.WAD@{offset}:{size}',wav_bytes(pcm,48000),48000,len(pcm))
            except ValueError as ex: errors.append({'name':name,'error':str(ex)})

# The public rip supplies the ten ambience tracks, not licensed soundtrack songs.
print('[DISNEY_PROGRESS] 85 Catalogando musicas e ambientes...', flush=True)
missing_music=[]
for offset,size,name in hed_entries(GAME_DIR/'MUSIC/MUSIC.HED'):
    stem=name.rsplit('/',1)[-1]
    if stem in gc:
        hit=gc[stem];install('music/'+stem,f'MUSIC.WAD@{offset}:{size}',hit['data'],hit['rate'],hit['frames'])
    else: missing_music.append(name)

assert len({r[0] for r in rows})==len(rows)
(DEST/'catalog.tsv').write_text(''.join('\t'.join(r)+'\n' for r in rows),encoding='utf-8')
report={'source':BASE,'archives':archives,'wavInArchives':len(gc)+len(skipped),'englishAndNonVoiceWavs':len(gc),'otherLanguageWavsSkipped':len(skipped),'installedLogicalSounds':len(rows),'gamecubeNamesUsed':len(used),'unmatchedGamecubeNames':sorted(set(gc)-used),'missingMusic':missing_music,'conversionErrors':errors,'sounds':records,'limitations':['Name correspondence is established; GameCube and PS2 waveforms may differ.','Music soundtrack tracks absent from the public pack are not substituted.']}
report_path=ROOT/'analysis/audio-asset-installation.json'
report_path.parent.mkdir(parents=True,exist_ok=True)
report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('[DISNEY_PROGRESS] 100 Catalogo de audio base instalado.', flush=True)
print(json.dumps({k:v for k,v in report.items() if k!='sounds'},ensure_ascii=True,indent=2),flush=True)
if errors:sys.exit(2)
