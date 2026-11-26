// VKey - "Vietnamese only inside the game's text boxes" decisions
// SPDX-License-Identifier: GPL-3.0-only
//
// Per-app option AppOverrideEntry::chatOnly. In a game like League of Legends
// the hook must be transparent while the player is fighting (Q/W/E/R/D/F are
// abilities, Telex would swallow them) and behave normally while the chat box
// or another in-game text box has focus.
//
// Signal (measured in League of Legends, 2026-11-26): the game sets the IME
// conversion mode of its window to IME_CMODE_NATIVE (0x1) whenever one of its
// text boxes takes focus and back to 0 when the box loses focus - Enter,
// Shift+Enter, Esc and clicking back onto the map all flip it within one
// frame. It is read from outside the game with WM_IME_CONTROL /
// IMC_GETCONVERSIONMODE sent to the window's default IME window, so nothing
// is injected into the game.
//
// Fail-safe direction: anything we cannot read counts as "no text box", i.e.
// pass keys through. Losing Vietnamese in chat is a nuisance; eating an
// ability key mid-fight is not acceptable.
//
// Linux-portable, no Win32 headers.

#pragma once

#include <cstdint>

namespace NextKey {

/// IME_CMODE_NATIVE from <imm.h>, duplicated to keep this header portable.
inline constexpr std::int64_t kImeConversionNative = 0x0001;

/// Interpret one IMC_GETCONVERSIONMODE probe of the foreground game window.
/// `queryOk` is false when there was no IME window or SendMessageTimeout
/// failed / timed out (game busy, window switching).
[[nodiscard]] constexpr bool DecodeGameTextFocus(bool queryOk,
                                                 std::int64_t conversionMode) noexcept {
    return queryOk && (conversionMode & kImeConversionNative) != 0;
}

/// Smooths the probe stream. League of Legends regularly misses the 30 ms
/// SendMessageTimeout while it renders (ERROR_TIMEOUT about once a second in
/// the 2026-11-26 log), and treating each miss as "no text box" would close
/// the gate for one tick in the middle of a chat message — dropping the word
/// being typed. A failed probe therefore keeps the last answer, until probes
/// have failed for kFailHoldMs in a row; then it falls back to "no text box".
class GameTextFocusFilter {
public:
    static constexpr std::uint64_t kFailHoldMs = 200;

    /// Feed one probe taken at `nowMs`; returns the filtered "text box focused".
    bool Update(bool queryOk, std::int64_t conversionMode, std::uint64_t nowMs) noexcept {
        if (queryOk) {
            focused_ = DecodeGameTextFocus(true, conversionMode);
            failing_ = false;
            return focused_;
        }
        if (!failing_) {
            failing_ = true;
            failSinceMs_ = nowMs;
        }
        if (nowMs - failSinceMs_ >= kFailHoldMs) focused_ = false;
        return focused_;
    }

    /// The app lost focus / stopped being chat-gated: forget everything.
    void Reset() noexcept {
        focused_ = false;
        failing_ = false;
        failSinceMs_ = 0;
    }

    [[nodiscard]] bool IsFailing() const noexcept { return failing_; }

private:
    bool focused_ = false;
    bool failing_ = false;
    std::uint64_t failSinceMs_ = 0;
};

/// True when the hook must pass the key straight to the app: the focused app
/// is chat-gated and none of its text boxes currently has focus.
[[nodiscard]] constexpr bool ShouldPassThroughForGameChat(bool isChatGatedApp,
                                                          bool textBoxFocused) noexcept {
    return isChatGatedApp && !textBoxFocused;
}

}  // namespace NextKey
