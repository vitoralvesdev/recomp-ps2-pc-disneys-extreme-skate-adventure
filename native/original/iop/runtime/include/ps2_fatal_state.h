#pragma once

#include <atomic>

namespace ps2::runtime
{
    inline std::atomic<bool> &fatalErrorState() noexcept
    {
        static std::atomic<bool> state{false};
        return state;
    }
}
