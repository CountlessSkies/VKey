// VKey - background probe for "a text box inside the game has focus"
// SPDX-License-Identifier: GPL-3.0-only

#include "app/system/GameTextFocusPoller.h"

#include <Windows.h>
#include <imm.h>

#include <chrono>

#include "core/GameChatGateDecision.h"
#include "core/Logger.h"

#pragma comment(lib, "imm32.lib")

namespace NextKey {

namespace {

// IMC_GETCONVERSIONMODE (imm.h, only defined with _IMM_SDK / immdev.h).
constexpr WPARAM kImcGetConversionMode = 0x0001;

bool ProbeForegroundTextFocus() {
    HWND fg = GetForegroundWindow();
    if (!fg) return DecodeGameTextFocus(false, 0);
    HWND ime = ImmGetDefaultIMEWnd(fg);
    if (!ime) return DecodeGameTextFocus(false, 0);
    DWORD_PTR result = 0;
    const LRESULT ok = SendMessageTimeoutW(ime, WM_IME_CONTROL, kImcGetConversionMode, 0,
                                           SMTO_ABORTIFHUNG,
                                           GameTextFocusPoller::kProbeTimeoutMs, &result);
    return DecodeGameTextFocus(ok != 0, static_cast<std::int64_t>(result));
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
    bool last = false;
    textFocus_.store(false, std::memory_order_release);
    for (;;) {
        const bool active = enabled_.load(std::memory_order_acquire);
        const bool focused = active && ProbeForegroundTextFocus();
        if (focused != last) {
            textFocus_.store(focused, std::memory_order_release);
            Logger::Log(L"[VKey] Game chat gate: text box %ls", focused ? L"focused (V)" : L"left (pass-through)");
            last = focused;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        const auto wait = std::chrono::milliseconds(active ? kActiveIntervalMs : kIdleIntervalMs);
        if (cv_.wait_for(lock, wait, [this] { return stopRequested_; })) break;
    }
}

}  // namespace NextKey
