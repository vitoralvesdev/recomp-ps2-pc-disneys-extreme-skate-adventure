from pathlib import Path
import tomllib, re, json
root=Path(__file__).resolve().parents[1]
cfg=tomllib.loads((root/'recomp/initial.toml').read_text(encoding='utf-8-sig'))
stubs_dir=root/'tools/PS2Recomp/ps2xRuntime/src/lib/Kernel/Stubs'
source_text='\n'.join(p.read_text(encoding='utf-8-sig') for p in stubs_dir.glob('*.cpp'))
manual={'FUN_002cdc88_0x2cdc88.cpp','FUN_002d7710_0x2d7710.cpp','FUN_002d7100_0x2d7100.cpp','FUN_002cebc0_0x2cebc0.cpp','FUN_002e9818_0x2e9818.cpp','FUN_002cf300_0x2cf300.cpp','FUN_002cf4d0_0x2cf4d0.cpp','FUN_002cf6c0_0x2cf6c0.cpp'}
selected=[]
file_names={'sceSifResetIop','sceSifSyncIop','sceSifRebootIop','sceSifInitIopHeap','sceSifAllocIopHeap','sceSifLoadIopHeap','sceFsInit','sceFsReset','sceOpen','sceClose','sceLseek','sceRead','sceWrite','sceIoctl'}
handler_aliases={'_sceMpegFlush':'sceMpegFlush'}
for value in cfg['general']['stubs']:
    name,address=value.split('@')
    handler=handler_aliases.get(name,name)
    if not (name.startswith('scePad') or name.startswith('sceCd') or handler.startswith('sceMpeg') or name in file_names): continue
    match=re.search(r'void\s+'+re.escape(handler)+r'\s*\([^)]*\)\s*\{',source_text)
    if not match: continue
    start=match.end(); depth=1; end=start
    while depth and end<len(source_text):
        depth += (source_text[end]=='{')-(source_text[end]=='}'); end+=1
    if 'TODO_NAMED' in source_text[start:end]: continue
    address=int(address,16)
    files=list((root/'src/generated-ghidra').glob(f'*_0x{address:x}.cpp'))
    if len(files)!=1: continue
    file=files[0]
    if f'ps2_stubs::{handler}(' in file.read_text(encoding='utf-8-sig'): continue
    selected.append({'file':file.name,'handler':handler,'address':hex(address)})
text='#include "ps2_runtime.h"\n#include "ps2_stubs.h"\n#include "ps2_recompiled_functions.h"\n'
for item in selected:
    symbol=item['file'][:-4]
    text+=f'void {symbol}(uint8_t* ram, R5900Context* ctx, PS2Runtime* rt) {{ ctx->pc=getRegU32(ctx,31); ps2_stubs::{item["handler"]}(ram,ctx,rt); }}\n'
(root/'src/host/platform_bridges.cpp').write_text(text,encoding='utf-8')
(root/'recomp/host-bridges.json').write_text(json.dumps({'edition':'SLUS_206.07 USA','evidence':'SDK signatures from initial.toml; functional runtime handlers only','excludedSources':sorted(manual|{i['file'] for i in selected}),'platformBindings':selected},indent=2),encoding='utf-8')
excluded=manual|{i['file'] for i in selected}
for batch in (root/'build/guest-batches').glob('*.cpp'):
    old=batch.read_text(encoding='utf-8-sig')
    new=''.join(line for line in old.splitlines(keepends=True) if not any('/'+name+'"' in line for name in excluded))
    if old!=new: batch.write_text(new,encoding='utf-8')
print(f'{len(selected)} functional platform bridges prepared')



