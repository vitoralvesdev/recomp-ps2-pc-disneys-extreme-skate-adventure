// Runs only compiled original IRX module initializers and their native services.
// Does not launch the game, VU interpreters, PCSX2, or the resource laboratory.
#include "emulator/iop_emulator.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>
namespace fs = std::filesystem;
using namespace ps2x::iop;
class BootHost final : public IopHost {
    std::vector<uint8_t> ee = std::vector<uint8_t>(32u * 1024u * 1024u);
    std::map<uint64_t, std::vector<uint8_t>> files;
    uint64_t nextHandle = 1;
public:
    fs::path root;
    unsigned errors = 0;
    bool readGuest(uint32_t a, void *d, size_t s) const override {
        if (a > ee.size() || s > ee.size() - a) return false;
        if (s) std::memcpy(d, ee.data() + a, s); return true;
    }
    bool writeGuest(uint32_t a, const void *d, size_t s) override {
        if (a > ee.size() || s > ee.size() - a) return false;
        if (s) std::memcpy(ee.data() + a, d, s); return true;
    }
    bool zeroGuest(uint32_t a, size_t s) override {
        if (a > ee.size() || s > ee.size() - a) return false;
        if (s) std::memset(ee.data() + a, 0, s); return true;
    }
    bool normalizeGuestAddress(uint32_t a, uint32_t &p) const override {
        p = a & 0x1fffffffu; return p < ee.size();
    }
    uint32_t allocateIopHandle(IopHandleKind) override { throw std::runtime_error("Unexpected host handle request"); }
    uint32_t allocateGuest(uint32_t, uint32_t) override { throw std::runtime_error("Unexpected EE allocation"); }
    void freeGuest(uint32_t) override { throw std::runtime_error("Unexpected EE free"); }
    void audioCommand(uint32_t, uint32_t, GuestBuffer, GuestBuffer) override {
        throw std::runtime_error("Audio endpoint is not connected in this boot test");
    }
    std::string hostPath(HostPathKind) const override { return root.string(); }
    std::string translateGuestPath(std::string_view p) const override { return std::string(p); }
    uint64_t openHostFile(std::string_view p) override {
        const auto name = fs::u8path(p);
        std::ifstream stream(name, std::ios::binary);
        if (!stream) return 0;
        auto &bytes = files[nextHandle];
        bytes.assign(std::istreambuf_iterator<char>(stream), {});
        return nextHandle++;
    }
    bool hostFileSize(uint64_t h, uint64_t &size) const override {
        auto it = files.find(h); if (it == files.end()) return false;
        size = it->second.size(); return true;
    }
    bool readHostFile(uint64_t h, uint64_t off, void *d, size_t s, size_t &read) override {
        read = 0; auto it = files.find(h);
        if (it == files.end() || off > it->second.size()) return false;
        read = std::min(s, it->second.size() - static_cast<size_t>(off));
        if (read) std::memcpy(d, it->second.data() + off, read); return true;
    }
    void closeHostFile(uint64_t h) override { files.erase(h); }
    int32_t memoryCard(const MemoryCardRequest &) override { throw std::runtime_error("Memory card endpoint not connected in boot test"); }
    bool hasGuestFunction(uint32_t) const override { return false; }
    bool invokeGuestFunction(uint64_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t *) override { return false; }
    void log(LogLevel level, std::string_view message) override {
        std::cout << message << '\n';
        if (level == LogLevel::Error) ++errors;
    }
};
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc != 2) throw std::runtime_error("Usage: disney_iop_native_boot_test <original IOP directory>");
        BootHost host; host.root = fs::path(argv[1]);
        ps2x::iop::detail::IopEmulator native(host);
        constexpr std::array names = {"SIO2MAN.IRX", "PADMAN.IRX", "LIBSD.IRX", "SDRDRV.IRX", "MCMAN.IRX", "MCSERV.IRX", "EZPCM.IRX"};
        for (const auto *name : names) {
            const auto path = (host.root / name).u8string();
            const auto before = native.instructions();
            const auto result = native.loadModule(std::string(reinterpret_cast<const char *>(path.data()), path.size()), nullptr, 0);
            if (result.moduleId < 0 || (result.startResult != 0 && result.startResult != 2))
                throw std::runtime_error(std::string("Original initializer failed: ") + name);
            if (!native.nativeFailure().empty()) throw std::runtime_error(native.nativeFailure());
            std::cout << "Native initialization completed: " << name << " instructions=" << native.instructions() - before << '\n';
        }
        for (unsigned i=0; i<60; ++i) {
            native.runEeCycles(294912000u / 60u);
            if (!native.nativeFailure().empty()) throw std::runtime_error(native.nativeFailure());
        }
        if (host.errors) throw std::runtime_error("Native services logged an initialization error");
        std::cout << "PASS: " << native.loadedModuleCount() << " original IRX initializers; "
                  << native.threadCount() << " threads; " << native.rpcServerCount() << " RPC servers; "
                  << native.instructions() << " compiled instructions; no instruction interpreter linked\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
