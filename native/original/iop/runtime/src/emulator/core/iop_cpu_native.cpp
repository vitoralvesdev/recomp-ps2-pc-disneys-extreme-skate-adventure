#include "iop_cpu.h"
#include "native/iop_native_exceptions.h"
namespace ps2x::iop::detail {
IopCpuCore::IopCpuCore(IopMemory &memory) noexcept : m_memory(memory) {}
void IopCpuCore::raiseException(IopCpuState &cpu, uint32_t code, uint32_t pc,
                              bool delay, std::optional<uint32_t> address) const {
    iopRaiseException(cpu, code, pc, delay, address);
}
}
