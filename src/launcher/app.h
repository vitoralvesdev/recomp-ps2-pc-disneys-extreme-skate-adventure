#pragma once

#include "common.h"

#include <memory>

namespace ds {

class App {
public:
    static App& instance();
    ~App();

    bool initialize(HINSTANCE instance, bool diagnosticsMode);
    int run();
    void shutdown();

    HWND window() const;
    static const wchar_t* className();

private:
    App();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ds
