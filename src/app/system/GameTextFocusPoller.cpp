// VKey - background probe for "a text box inside the game has focus"
// SPDX-License-Identifier: GPL-3.0-only

#include "app/system/GameTextFocusPoller.h"

#include <Windows.h>
#include <imm.h>

#include <chrono>
#include <cstdint>

#include "core/GameChatGateDecision.h"
#include "core/Logger.h"

#pragma comment(lib, "imm32.lib")

namespace NextKey {

namespace {

// IMC_GETCONVERSIONMODE (imm.h, only defined with _IMM_SDK / immdev.h).
constexpr WPARAM kImcGetConversionMode = 0x0001;

struct ProbeResult {
    HWND fg = nullptr;
    HWND ime = nullptr;
    bool ok = false;
    std::int64_t conv = -1;
    DWORD error = 0;
};

ProbeResult ProbeForeground() {
    ProbeResult r;
    r.fg = GetForegroundWindow();
    if (!r.fg) return r;
    r.ime = ImmGetDefaultIMEWnd(r.fg);
    if (!r.ime) {
        r.error = GetLastError();
        return r;
    }
    DWORD_PTR result = 0;
    SetLastError(0);
    const LRESULT sent = SendMessageTimeoutW(r.ime, WM_IME_CONTROL, kImcGetConversionMode, 0,
                                             SMTO_ABORTIFHUNG,
                                             GameTextFocusPoller::kProbeTimeoutMs, &result);
    r.ok = sent != 0;
    r.conv = r.ok ? static_cast<std::int64_t>(result) : -2;
    if (!r.ok) r.error = GetLastError();
    return r;
}

}  // namespace

GameTextFocusPoller::GameTextFocusPoller(const std::atomic<bool>& enabled,
                                         std::atomic<bool>& textFocus)
    : enabled_(enabled), textFocus_(textFocus) {}

GameTextFocusPoller::~GameTextFocusPoller() {
    Stop();
}

void GameTextFocusPoller::Start() {
    if (thread_.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopRequested_ = false;
    }
    thread_ = std::thread([this] { Run(); });
}

void GameTextFocusPoller::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopRequested_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    textFocus_.store(false, std::memory_order_release);
}

void GameTextFocusPoller::Run() {
    Logger::Log(L"[VKey] Game chat gate: poller thread started");
    bool last = false;
    bool lastActive = false;
    bool lastFailing = false;
    GameTextFocusFilter filter;
    textFocus_.store(false, std::memory_order_release);
    for (;;) {
        const bool active = enabled_.load(std::memory_order_acquire);
        if (active != lastActive) {
            Logger::Log(L"[VKey] Game chat gate: %ls", active ? L"chat-gated app focused, probing"
                                                            : L"idle (no chat-gated app)");
            lastActive = active;
            filter.Reset();
        }

        bool focused = false;
        if (active) {
            const ProbeResult p = ProbeForeground();
            focused = filter.Update(p.ok, p.conv, GetTickCount64());
            // A failure streak long enough to close the gate is worth a line;
            // single timeouts while the game renders are routine and are not.
            const bool failingLong = filter.IsFailing() && !p.ok && !focused && last;
            if (failingLong && !lastFailing) {
                Logger::Log(L"[VKey] Game chat gate: probe failing (ime=%p conv=%lld err=%lu), "
                            L"falling back to pass-through",
                            static_cast<void*>(p.ime), static_cast<long long>(p.conv), p.error);
            }
            lastFailing = failingLong;
        }

        if (focused != last) {
            textFocus_.store(focused, std::memory_order_release);
            Logger::Log(L"[VKey] Game chat gate: text box %ls",
                        focused ? L"focused (V)" : L"left (pass-through)");
            last = focused;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        const auto wait = std::chrono::milliseconds(active ? kActiveIntervalMs : kIdleIntervalMs);
        if (cv_.wait_for(lock, wait, [this] { return stopRequested_; })) break;
    }
}

}  // namespace NextKey
