from pathlib import Path
import re,json
root=Path(__file__).resolve().parents[1]
records=json.loads((root/'analysis/disjoint-initial-functions.json').read_text(encoding='utf-8'))
by_file={r['file']:r for r in records}
for item in json.loads((root/'recomp/recovered-overlap-entries.json').read_text(encoding='utf-8')):
 if item['file'] in by_file: by_file[item['file']]['entryPoints']=item['entryPoints']
 else: records.append(item)
output=root/'src/recovered'; output.mkdir(exist_ok=True)
registration='#include "ps2_runtime.h"\n'
registrations=[]
for record in records:
 text=(root/'src/generated-initial'/record['file']).read_text(encoding='utf-8-sig')
 symbol=re.search(r'void (\w+)\(uint8_t\*',text).group(1)
 for value in record.get('entryPoints',[]):
  target=int(value,16)
  if target==int(record['start'],16): continue
  label=f'label_{target:x}'
  if label+':' not in text: text=text.replace(f'    // 0x{target:x}:',label+':\n'+f'    // 0x{target:x}:',1)
  selection=re.search(r'switch \(ctx->pc\) \{(.*?)default: break;\s*\}',text,re.S)
  if selection:
   if not re.search(r'case 0x'+f'{target:x}'+r'u:',selection.group(1),re.I):
    text=text.replace('    switch (ctx->pc) {',f'    switch (ctx->pc) {{\n        case 0x{target:x}u: goto {label};',1)
  else:
   signature=f'void {symbol}(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {{'
   assert signature in text
   text=text.replace(signature,signature+f'\n    switch(ctx->pc) {{ case 0x{target:x}u: goto {label}; default: break; }}',1)
 text=re.sub(r'\b\w+_0x([a-fA-F0-9]+)\(rdram, ctx, runtime\);\s*return;',lambda m:f'runtime->dispatchGuestBranch(rdram, ctx, 0x{m.group(1)}u, ctx->pc, 0u, PS2Runtime::GuestBranchKind::DirectJump, "recovered tail"); return;',text)
 destination=output/record['file']
 if not destination.exists() or destination.read_text(encoding='utf-8-sig')!=text: destination.write_text(text,encoding='utf-8')
 registration+=f'void {symbol}(uint8_t*,R5900Context*,PS2Runtime*);\n'
 entries={int(record['start'],16)} | {int(value,16) for value in record.get('entryPoints',[])}
 selection=re.search(r'switch \(ctx->pc\) \{(.*?)default: break;\s*\}',text,re.S)
 if selection: entries.update(int(v,16) for v in re.findall(r'case (0x[0-9a-fA-F]+)u:',selection.group(1)))
 for entry in sorted(entries): registrations.append((entry,symbol))
registration+='void registerDisneyRecoveredFunctions(PS2Runtime& runtime) {\n'
for entry,symbol in registrations:
 registration+=f' if (!runtime.hasFunction(0x{entry:x}u)) runtime.registerFunction(0x{entry:x}u,&{symbol});\n'
registration+='}\n'
(root/'src/host/recovered_functions.cpp').write_text(registration,encoding='utf-8')
for index in range(0,len(records),64):
 text=''.join('#include "'+record['file']+'"\n' for record in records[index:index+64])
 (output/f'recovered_batch_{index//64:02}.cpp').write_text(text,encoding='utf-8')
print(f'{len(records)} disjoint functions, {len(registrations)} callable entries restored')

