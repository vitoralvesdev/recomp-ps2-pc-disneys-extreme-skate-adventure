#include "config.h"

#include <algorithm>
#include <cwchar>
#include <sstream>

namespace ds {
namespace {

std::string serialize(const Settings& settings) {
    std::ostringstream out;
    out << "; Disney's Extreme Skate Adventure - configuracoes do launcher\r\n";
    out << "; Este arquivo pode ser editado manualmente. Caminhos em UTF-8.\r\n";
    out << "[launcher]\r\n";
    out << "last_iso=" << toUtf8(settings.lastIsoPath) << "\r\n";
    out << "render_scale=" << settings.renderScale << "\r\n";
    out << "renderer=" << (settings.gpuRenderer ? "gpu" : "software") << "\r\n";
    out << "native_vblank=" << (settings.nativeVblank ? 1 : 0) << "\r\n";
    out << "hang_diagnostics=" << (settings.hangDiagnostics ? 1 : 0) << "\r\n";
    out << "check_updates=" << (settings.checkUpdatesOnStart ? 1 : 0) << "\r\n";
    out << "source_root=" << toUtf8(settings.sourceRoot) << "\r\n";
    return out.str();
}

bool parseBool(const std::string& value, bool fallback) {
    const std::string normalized = trimUtf8(value);
    if (normalized == "1" || normalized == "true" || normalized == "yes") return true;
    if (normalized == "0" || normalized == "false" || normalized == "no") return false;
    return fallback;
}

} // namespace

const wchar_t* renderScaleLabel(int scale) {
    switch (scale) {
    case 1: return L"1x (640 x 448)";
    case 2: return L"2x (1280 x 896)";
    case 3: return L"3x (1920 x 1344)";
    default: return L"1x (640 x 448)";
    }
}

bool Config::load(const fs::path& path, std::wstring& error) {
    path_ = path;
    error.clear();
    std::error_code ec;
    if (!fs::exists(path, ec)) return true;
    std::vector<uint8_t> bytes;
    if (!readFileBytes(path, bytes, 1024 * 1024, error)) return false;
    std::string text(bytes.begin(), bytes.end());
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef &&
        static_cast<unsigned char>(text[1]) == 0xbb &&
        static_cast<unsigned char>(text[2]) == 0xbf) {
        text.erase(0, 3);
    }
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        line = trimUtf8(line);
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        const size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = trimUtf8(line.substr(0, equals));
        const std::string value = trimUtf8(line.substr(equals + 1));
        if (key == "last_iso") {
            settings_.lastIsoPath = toWide(value);
        } else if (key == "render_scale") {
            try {
                settings_.renderScale = std::clamp(std::stoi(value), 1, 3);
            } catch (...) {
                settings_.renderScale = 1;
            }
        } else if (key == "renderer") {
            settings_.gpuRenderer = value != "software";
        } else if (key == "native_vblank") {
            settings_.nativeVblank = parseBool(value, true);
        } else if (key == "hang_diagnostics") {
            settings_.hangDiagnostics = parseBool(value, false);
        } else if (key == "check_updates") {
            settings_.checkUpdatesOnStart = parseBool(value, true);
        } else if (key == "source_root") {
            settings_.sourceRoot = toWide(value);
        }
    }
    return true;
}

bool Config::save(const fs::path& path, std::wstring& error) const {
    return writeTextFileAtomic(path, serialize(settings_), error);
}

} // namespace ds
