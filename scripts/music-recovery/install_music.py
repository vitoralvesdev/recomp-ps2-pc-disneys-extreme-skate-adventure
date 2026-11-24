from pathlib import Path
from datetime import date
import hashlib, json, math, shutil, sys, wave

p = Path(sys.argv[1]).resolve()
w = Path(sys.argv[2]).resolve()
dest = p / 'assets/native/audio'
old = json.loads((p / 'analysis/audio-asset-installation.json').read_text(encoding='utf-8'))
missing = set(old['missingMusic'])
rows = [r.split('\t') for r in (dest / 'catalog.tsv').read_text(encoding='utf-8').splitlines()]
keys = {a[0] for a in rows}
recovered = [a for a in old['sounds'] if a.get('source', {}).get('kind') == 'PS2 ISO stereo ADPCM converted to PCM']
music_keys = {key for key in keys if key.startswith('music/')}

if not missing:
    if len(recovered) == len(music_keys) == 37:
        for entry in recovered:
            assert hashlib.sha256((dest / entry['file']).read_bytes()).hexdigest() == entry['sha256']
        print('37 music entries already installed; recovered PCM hashes verified.')
        raise SystemExit(0)
    raise ValueError('Catálogo de áudio antigo detectado; execute Instalar-Audios-Originais.py antes da recuperação.')

if len(missing) != 37:
    raise ValueError(f'Esperadas 37 músicas ausentes para a conversão exclusiva da ISO; encontradas {len(missing)}.')

stats = []
for line in (w / 'decoded.tsv').read_text(encoding='utf-8-sig').splitlines():
    name, offset, size, frames, rate, clips = line.split('\t')
    frames = int(frames)
    file = w / 'pcm' / f'{name}.wav'
    with wave.open(str(file), 'rb') as f:
        assert f.getnchannels() == 2 and f.getsampwidth() == 2 and f.getframerate() == 48000
        assert f.getnframes() == frames
        assert len(f.readframes(frames)) == frames * 4
    key = 'music/' + name
    stats.append({'key': key, 'offset': int(offset), 'logicalCompressedBytes': int(size),
                  'physicalReadBytes': math.ceil(int(size) / 0x30000) * 0x30000,
                  'frames': frames, 'rate': 48000, 'channels': 2,
                  'decoderClippingCount': int(clips), 'sha256': hashlib.sha256(file.read_bytes()).hexdigest(),
                  'durationSeconds': frames / 48000})

assert len(stats) == 37 and {s['key'] for s in stats} == missing
backups = w / 'before'
backups.mkdir(exist_ok=True)
for source in [dest / 'catalog.tsv', p / 'analysis/audio-asset-installation.json']:
    target = backups / source.name
    if not target.exists():
        shutil.copyfile(source, target)

for rec in stats:
    name = rec['key'].split('/')[-1]
    filename = 'files/' + rec['sha256'] + '.wav'
    target = dest / filename
    if target.exists():
        assert hashlib.sha256(target.read_bytes()).hexdigest() == rec['sha256']
    else:
        shutil.copyfile(w / 'pcm' / f'{name}.wav', target)
    rows.append([rec['key'], 'assets/native/audio/' + filename, '0', '0'])
    old['sounds'].append({'key': rec['key'],
                          'original': f"MUSIC.WAD@{rec['offset']}:{rec['logicalCompressedBytes']}",
                          'file': filename, 'sha256': rec['sha256'], 'rate': 48000,
                          'frames': rec['frames'], 'loopStart': 0, 'loopEnd': 0,
                          'source': {'kind': 'PS2 ISO stereo ADPCM converted to PCM',
                                     'interleaveBytes': 0x18000, 'physicalPaddingRead': True}})

assert len(rows) == len({a[0] for a in rows})
old['installedLogicalSounds'] = len(rows)
old['missingMusic'] = []
old['limitations'] = [x for x in old.get('limitations', []) if 'Music soundtrack tracks absent' not in x]
old['musicRecovery'] = {'date': date.today().isoformat(), 'newTracks': len(missing),
                        'totalMusicKeys': 37, 'source': 'Local extracted MUSIC/MUSIC.WAD from SLUS_206.07',
                        'validation': '37 RIFF PCM stereo files decoded from MUSIC.WAD.'}
temporary = dest / 'catalog.tsv.music-recovery.tmp'
temporary.write_text(''.join('\t'.join(r) + '\n' for r in rows), encoding='utf-8')
temporary.replace(dest / 'catalog.tsv')
(p / 'analysis/audio-asset-installation.json').write_text(
    json.dumps(old, ensure_ascii=False, indent=2), encoding='utf-8')
report = {'newTracks': len(missing), 'totalMusicKeys': 37, 'catalogEntries': len(rows),
          'tracks': stats, 'source': old['musicRecovery'], 'backup': str(backups)}
(w / 'music-installation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
(p / 'analysis/music-recovery-2026-10-25.json').write_text(
    json.dumps(report, indent=2, allow_nan=False), encoding='utf-8')
print(json.dumps({k: report[k] for k in ['newTracks', 'totalMusicKeys', 'catalogEntries']}, indent=2))
