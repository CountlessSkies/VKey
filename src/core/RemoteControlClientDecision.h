// VKey - Pure decision: is the focused app a remote-control client window
// whose keys must reach the remote machine untouched?
// SPDX-License-Identifier: GPL-3.0-only
//
// ── The problem ──
// RustDesk (keyboard "map" mode, and in practice also "translate") forwards
// physical key events to the remote machine but drops the KEYEVENTF_UNICODE
// characters VKey injects. When VKey's low-level hook runs before RustDesk's
// (VKey started after RustDesk), typing "laf" makes VKey send Backspace + U+00E0:
// the Backspace reaches the remote, the "à" does not → "l" instead of "là".
// Whether it works depended on which app was started last.
//
// ── The fix ──
// While a listed client window has focus, VKey passes every key through
// unchanged (the logical V/E mode is not touched). The remote machine's own
// IME composes the text — the setup where both machines run VKey in V mode.
//
// Only clients verified to drop VKey's Unicode output are listed. Windows
// Remote Desktop (mstsc) forwards Unicode and is used to type Vietnamese into
// machines without an IME, so it is deliberately absent.

#pragma once

#include <array>
#include <string_view>

namespace NextKey {

// Lower-case executable names, matching FocusClassification::exeName.
inline constexpr std::array<std::wstring_view, 1> kRemoteControlClientExes = {
    L"rustdesk.exe",
};

[[nodiscard]] constexpr bool IsRemoteControlClientExe(std::wstring_view exeLower) noexcept {
    for (const auto name : kRemoteControlClientExes) {
        if (exeLower == name) return true;
    }
    return false;
}

}  // namespace NextKey
