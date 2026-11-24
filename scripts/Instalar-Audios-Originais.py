"""Convert VAG effects and voice streams from the user's own SLUS_206.07 extraction.

Music is decoded separately by scripts/music-recovery after the base catalog is
created. No external audio packages are downloaded or required.
"""
from pathlib import Path, PurePosixPath
from collections import defaultdict
from array import array
import argparse, hashlib, io, json, struct, sys, wave

FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))


class AudioError(ValueError):
    pass


sha = lambda b: hashlib.sha256(b).hexdigest()


def progress(percent, message):
    single_line = message.replace('\r', ' ').replace('\n', ' ')
    print(f'[DISNEY_PROGRESS] {percent} {single_line}', flush=True)


def decode_adpcm(data):
    pcm = array('h')
    old = older = 0
    start = end = None
    for at in range(0, len(data) - 15, 16):
        b = data[at:at + 16]
        shift, filt = b[0] & 15, b[0] >> 4
        if shift > 12 or filt > 4:
            raise ValueError(f'Invalid ADPCM at {at}: {b[:2].hex()}')
        flag = b[1]
        if flag & 4:
            start = len(pcm)
        a, c = FILTERS[filt]
        for v in b[2:]:
            for n in (v & 15, v >> 4):
                n = n - 16 if n >= 8 else n
                sample = (n << 12) >> shift
                sample += (old * a + older * c + 32) >> 6
                sample = max(-32768, min(32767, sample))
                older, old = old, sample
                pcm.append(sample)
        if flag & 1:
            if flag & 2:
                end = len(pcm)
            break
    if sys.byteorder != 'little':
        pcm.byteswap()
    return pcm, start, end


def wav_bytes(pcm, rate):
    f = io.BytesIO()
    with wave.open(f, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())
    return f.getvalue()


def hed_entries(path):
    b = path.read_bytes()
    at = 0
    while at + 8 <= len(b):
        offset, size = struct.unpack_from('<II', b, at)
        if offset == 0xffffffff:
            break
        end = b.find(b'\0', at + 8)
        if end < 0:
            raise ValueError('Unterminated HED name')
        name = b[at + 8:end].decode('ascii').replace('\\', '/').strip('/').lower()
        yield offset, size, name
        at = (end + 4) & ~3


def source_vags(root):
    vags = defaultdict(list)
    for path in (root / 'assets/source/wad').rglob('*.vag'):
        normalized = str(path).replace('\\', '/').lower()
        if '/sounds/vag/' not in normalized:
            raise AudioError(f'Caminho VAG inesperado: {path}')
        tail = normalized.split('/sounds/vag/', 1)[1][:-4]
        vags[tail].append(path)
    if not vags:
        raise AudioError('Nenhum VAG encontrado em assets/source/wad; execute Preparar-Dados-Nativos.ps1 primeiro')
    return vags


def required_game_files(game_dir):
    required = [game_dir / 'STREAMS/STREAMS.HED', game_dir / 'STREAMS/STREAMS.WAD',
                game_dir / 'MUSIC/MUSIC.HED', game_dir / 'MUSIC/MUSIC.WAD']
    missing = [str(path) for path in required if not path.is_file() or path.stat().st_size == 0]
    if missing:
        raise AudioError('Arquivos extraídos ausentes ou vazios: ' + ', '.join(missing))
    return required


def validate_sources(root, game_dir):
    required = required_game_files(game_dir)
    vags = source_vags(root)
    streams = list(hed_entries(game_dir / 'STREAMS/STREAMS.HED'))
    music = list(hed_entries(game_dir / 'MUSIC/MUSIC.HED'))
    for wad, entries in ((game_dir / 'STREAMS/STREAMS.WAD', streams),
                         (game_dir / 'MUSIC/MUSIC.WAD', music)):
        size = wad.stat().st_size
        for offset, length, name in entries:
            if not name or offset > size or length > size - offset:
                raise AudioError(f'Intervalo inválido em {wad.name}: {name}')
    return {'vagFiles': sum(len(paths) for paths in vags.values()),
            'logicalVags': len(vags), 'streams': len(streams), 'music': len(music),
            'requiredFiles': len(required)}


def install_audio(root, game_dir):
    required_game_files(game_dir)
    vags = source_vags(root)
    dest = root / 'assets/native/audio'
    dest.mkdir(parents=True, exist_ok=True)
    rows, records, errors = [], [], []

    def install(key, original, data, rate, frames, loop_start=0, loop_end=0):
        digest = sha(data)
        filename = f'files/{digest}.wav'
        target = dest / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            target.write_bytes(data)
        if loop_end <= loop_start:
            loop_start = loop_end = 0
        rows.append((key, 'assets/native/audio/' + filename, str(loop_start), str(loop_end)))
        records.append({'key': key, 'original': original, 'file': filename, 'sha256': digest,
                        'rate': rate, 'frames': frames, 'loopStart': loop_start, 'loopEnd': loop_end,
                        'source': {'kind': 'PS2 ISO ADPCM converted to PCM'}})

    progress(55, 'Convertendo efeitos VAG extraídos...')
    for tail, paths in sorted(vags.items()):
        binaries = {sha(path.read_bytes()): path for path in paths}
        if len(binaries) != 1:
            raise ValueError(f'Different PS2 sounds share logical path: {tail}')
        path = paths[0]
        data = path.read_bytes()
        if data[:4] != b'VAGp' or len(data) < 48:
            raise ValueError(f'VAG inválido: {path}')
        length, rate = struct.unpack_from('>II', data, 12)
        if length <= 0 or length > len(data) - 48 or rate <= 0:
            raise ValueError(f'Cabeçalho VAG inválido: {path}')
        pcm, start, end = decode_adpcm(data[48:48 + length])
        if not pcm:
            raise ValueError(f'VAG sem amostras PCM: {path}')
        install('sfx/' + tail, str(path.relative_to(root)), wav_bytes(pcm, rate), rate, len(pcm),
                start or 0, end or 0)

    progress(75, 'Convertendo vozes e streams do disco...')
    stream_entries = list(hed_entries(game_dir / 'STREAMS/STREAMS.HED'))
    with (game_dir / 'STREAMS/STREAMS.WAD').open('rb') as stream:
        for offset, size, name in stream_entries:
            stream.seek(offset)
            data = stream.read(size)
            if len(data) != size:
                raise ValueError(f'STREAMS.WAD truncado em {name}')
            try:
                pcm, _, _ = decode_adpcm(data)
                if not pcm:
                    raise ValueError('stream sem amostras PCM')
                install('stream/' + name.removeprefix('streams/'),
                        f'STREAMS.WAD@{offset}:{size}', wav_bytes(pcm, 48000), 48000, len(pcm))
            except ValueError as ex:
                errors.append({'name': name, 'error': str(ex)})

    progress(85, 'Catalogando músicas para recuperação estéreo...')
    missing_music = [name for _, _, name in hed_entries(game_dir / 'MUSIC/MUSIC.HED')]
    if len({row[0] for row in rows}) != len(rows):
        raise AudioError('Chaves duplicadas no catálogo de áudio')
    (dest / 'catalog.tsv').write_text(''.join('\t'.join(row) + '\n' for row in rows), encoding='utf-8')
    report = {'source': 'Local SLUS_206.07 ISO extraction', 'gameDir': str(game_dir),
              'vagFiles': sum(len(paths) for paths in vags.values()), 'streamEntries': len(stream_entries),
              'musicEntries': len(missing_music), 'installedLogicalSounds': len(rows),
              'missingMusic': missing_music, 'conversionErrors': errors, 'sounds': records,
              'limitations': ['Áudio dos vídeos e reverberação completa continuam fora deste conversor.']}
    report_path = root / 'analysis/audio-asset-installation.json'
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    progress(100, 'Catálogo de áudio base instalado.')
    print(json.dumps({key: value for key, value in report.items() if key != 'sounds'},
                     ensure_ascii=True, indent=2), flush=True)
    if errors:
        raise AudioError(f'{len(errors)} stream(s) ADPCM não puderam ser convertidos; consulte analysis/audio-asset-installation.json')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', nargs='?', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--game-dir', type=Path)
    parser.add_argument('--validate-only', action='store_true',
                        help='valida os arquivos extraídos e os índices sem criar assets')
    args = parser.parse_args()
    root = args.root.resolve()
    game_dir = (args.game_dir or root / 'original/extracted').resolve()
    if args.validate_only:
        progress(100, json.dumps(validate_sources(root, game_dir), ensure_ascii=True))
        return 0
    install_audio(root, game_dir)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (AudioError, OSError, ValueError, struct.error, UnicodeError) as ex:
        print(f'ERRO: {ex}', file=sys.stderr, flush=True)
        sys.exit(2)
