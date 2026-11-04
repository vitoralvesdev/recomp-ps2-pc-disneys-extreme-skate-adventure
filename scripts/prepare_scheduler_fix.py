from pathlib import Path
import re,os,difflib
root=Path(__file__).resolve().parents[1]
source=root/'tools/PS2Recomp/ps2xRuntime/src/lib/Kernel/EeScheduler.cpp'
dest=root/'src/host/pc_scheduler.cpp'
original=source.read_text(encoding='utf-8-sig')
needle='''    processDueDeadlines();
    const uint32_t timerInterrupts'''
replacement='''    processDueDeadlines();
    // GS SIGNAL/FINISH set CSR in the GIF frontend. Deliver their unmasked
    // level to the game's registered INTC_GS handler at an EE safe point.
    // An in-flight invocation owns delivery until its guest handler returns.
    const auto &gs = m_runtime.memory().gs();
    const uint64_t pendingGs = gs.csr.load(std::memory_order_acquire) & 0x1fu;
    const uint64_t unmaskedGs = pendingGs & ~(gs.imr >> 8u);
    const GuestThread *irqOwner = currentThread();
    if (unmaskedGs != 0u && m_pendingInvocations.empty() &&
        (!irqOwner || irqOwner->invocations.empty()))
    {
        dispatchIrq(false, 0u);
    }
    const uint32_t timerInterrupts'''
assert needle in original
text=original.replace(needle,replacement,1)
text=text.replace('''    const int id = allocatePositiveId(nextId, handlers);''','''    const int id = allocatePositiveId(nextId, handlers);
    if (std::getenv("DISNEY_PORT_DIAGNOSTICS")) std::fprintf(stderr,"[irq-register] dmac=%d cause=%u handler=%08x available=%d\\n",dmac,cause,handler,m_runtime.hasFunction(handler));''',1)
text=text.replace('''                context.pc = 0u;''','''                if (std::getenv("DISNEY_PORT_DIAGNOSTICS")) std::fprintf(stderr,"[irq-missing-entry] pc=%08x\\n",context.pc);
                context.pc = 0u;''',1)
text=text.replace('''    for (const EeIrqHandler &handler : matching)
    {''','''    for (const EeIrqHandler &handler : matching)
    {
        static unsigned reports=0;
        if (std::getenv("DISNEY_PORT_DIAGNOSTICS") && reports++<24u) std::fprintf(stderr,"[irq-delivery] dmac=%d cause=%u handler=%08x\\n",dmac,cause,handler.handler);''',1)
text=text.replace('#include <cstring>','#include <cstring>\n#include <cstdio>\n#include <cstdlib>',1)
semantic=text
def relocate(m):
    candidate=source.parent/m.group(1)
    if candidate.is_file():return '#include "'+Path(os.path.relpath(candidate,dest.parent)).as_posix()+'"'
    return m.group(0)
text=re.sub(r'#include "([^"]+)"',relocate,text)
dest.write_text(text,encoding='utf-8')
(root/'docs/runtime-gs-interrupt-fix.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),semantic.splitlines(True),fromfile='upstream/EeScheduler.cpp',tofile='port/EeScheduler.cpp')),encoding='utf-8')

