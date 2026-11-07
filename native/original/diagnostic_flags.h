#pragma once
#include <cstdlib>

// Diagnostic flags are process settings supplied before runtime startup.
// Read each once: getenv locks and scans the CRT environment, including
// when tracing is disabled. Keep it out of VU/GIF/IRQ hot paths.
namespace disney::diagnostic_flags {
inline bool traceVu() {
    static const bool enabled = std::getenv("DISNEY_AOT_TRACE_VU") != nullptr;
    return enabled;
}
inline bool traceKick() {
    static const bool enabled = std::getenv("DISNEY_AOT_TRACE_KICK") != nullptr;
    return enabled;
}
inline bool traceChain() {
    static const bool enabled = std::getenv("DISNEY_AOT_TRACE_CHAIN") != nullptr;
    return enabled;
}
inline bool portDiagnostics() {
    static const bool enabled = std::getenv("DISNEY_PORT_DIAGNOSTICS") != nullptr;
    return enabled;
}
inline bool hangDiagnostics() {
    static const bool enabled = std::getenv("DISNEY_HANG_DIAGNOSTICS") != nullptr;
    return enabled;
}
}
