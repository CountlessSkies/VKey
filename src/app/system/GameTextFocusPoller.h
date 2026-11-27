// VKey - background probe for "a text box inside the game has focus"
// SPDX-License-Identifier: GPL-3.0-only
//
// Backs AppOverrideEntry::chatOnly. While the focused app is chat-gated
// (`enabled` true), a dedicated thread asks the foreground window's default
// IME window for its conversion mode every kActiveIntervalMs and publishes
// DecodeGameTextFocus() into `textFocus`. The hook thread only does an atomic
// load (Rule 11.2: no syscalls on the LL callback for this feature).
//
// The probe is a cross-process WM_IME_CONTROL sent with SendMessageTimeout on
// this thread, so a busy or hung game can delay the probe but never the hook.
// Every failure publishes false (pass keys through) — see
// core/GameChatGateDecision.h for why that is the safe side.

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "core/GameChatGateDecision.h"

namespace NextKey {

class GameTextFocusPoller {
public:
    static constexpr unsigned kActiveIntervalMs = 15;
    static constexpr unsigned kIdleIntervalMs = 100;
    static constexpr unsigned kProbeTimeoutMs = 30;

    GameTextFocusPoller(const std::atomic<bool>& enabled,
                        const std::atomic<bool>& alwaysVietnamese,
                        std::atomic<bool>& textFocus);
    ~GameTextFocusPoller();

    GameTextFocusPoller(const GameTextFocusPoller&) = delete;
    GameTextFocusPoller& operator=(const GameTextFocusPoller&) = delete;

    /// Called on the poller thread when DecideGameTrayState() changes.
    /// Set before Start(). The callback must only post.
    void SetTrayStateCallback(std::function<void(GameTrayState)> callback) {
        onTrayStateChanged_ = std::move(callback);
    }

    void Start();
    void Stop();

private:
    void Run();

    const std::atomic<bool>& enabled_;
    const std::atomic<bool>& alwaysVietnamese_;
    std::atomic<bool>& textFocus_;
    std::function<void(GameTrayState)> onTrayStateChanged_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopRequested_ = false;
};

}  // namespace NextKey
