from pathlib import Path
import re, os, difflib
root=Path(__file__).resolve().parents[1]
source=root/'tools/PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp'
dest=root/'src/host/pc_runtime.cpp'
original=source.read_text(encoding='utf-8-sig')
text=original
old='''    if (ctx->pc == entryPc)
    {
        ctx->pc = fallthroughPc;
    }'''
new='''    // A cooperative checkpoint can yield at the callee's entry address.
    // Generated returns and host bridges set PC explicitly; equal PC is not a return.
    if (ctx->pc == entryPc)
    {
        return false;
    }'''
assert old in text
text=text.replace(old,new,1)
text=text.replace('m_asyncCallbackStackTop = PS2_RAM_SIZE;', 'm_asyncCallbackStackTop = 0x00100000u; // Below the ELF; above synthetic kernel services')
text=text.replace('m_asyncCallbackStackFloor = std::min(kGuestHeapHardLimit, PS2_RAM_SIZE);','m_asyncCallbackStackFloor = 0x000c0000u;')
text=text.replace('m_asyncCallbackStackFloor = std::min(std::max(hardLimit, suggestedHeapBase), PS2_RAM_SIZE);','m_asyncCallbackStackFloor = 0x000c0000u;')
old_call='''    const uint32_t entryPc = ctx->pc;
    targetFn(rdram, ctx, this);'''
new_call='''    const uint32_t entryPc = ctx->pc;
    // Guest calls are resumable. Bound native recursion before invoking another
    // translated function, leaving PC at the target for the EE dispatcher.
    static thread_local unsigned guestCallDepth = 0u;
    if (guestCallDepth >= 64u)
    {
        return false;
    }
    struct GuestCallDepthGuard
    {
        unsigned &depth;
        explicit GuestCallDepthGuard(unsigned &value) : depth(value) { ++depth; }
        ~GuestCallDepthGuard() { --depth; }
    } depthGuard(guestCallDepth);
    targetFn(rdram, ctx, this);'''
assert old_call in text
text=text.replace(old_call,new_call,1)
semantic=text
def relocate(match):
    candidate=(source.parent/match.group(1))
    if candidate.is_file():
        return '#include "'+Path(os.path.relpath(candidate,dest.parent)).as_posix()+'"'
    return match.group(0)
text=re.sub(r'#include "([^"]+)"',relocate,text)
dest.write_text(text,encoding='utf-8')
(root/'docs/runtime-checkpoint-fix.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),semantic.splitlines(True),fromfile='upstream/ps2_runtime.cpp',tofile='port/ps2_runtime.cpp')),encoding='utf-8')



