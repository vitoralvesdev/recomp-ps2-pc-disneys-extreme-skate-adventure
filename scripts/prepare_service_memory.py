from pathlib import Path
import difflib
root=Path(__file__).resolve().parents[1]
path=root/'tools/PS2Recomp/ps2xRuntime/src/lib/Kernel/Syscalls/Helpers/State.h'
text=path.read_text(encoding='utf-8-sig')
original=text
# SLUS_206.07 uses its own top-down allocator, including RAM above 31 MiB.
# The synthetic kernel mirror ends at 0x80000 and the ELF starts at 0x100000.
for name,old,new in [('kRpcPacketPoolBase','0x01F00000','0x00080000'),('kRpcServerPoolBase','0x01F10000','0x00090000'),('kTlsPoolBase','0x01F20000','0x000A0000'),('kBootModePoolBase','0x01F30000','0x000B0000')]:
    before=f'{name} = {old};'; after=f'{name} = {new};'
    if before in text: text=text.replace(before,after,1)
    else: assert after in text, f'Unexpected upstream layout: {name}'
if text!=original:
    (root/'docs/runtime-service-memory-fix.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),text.splitlines(True),fromfile='upstream/State.h',tofile='port/State.h')),encoding='utf-8')
    path.write_text(text,encoding='utf-8')
print('Host service pools reserved below ELF: 0x80000..0xb1000; callback stacks 0xc0000..0x100000')
