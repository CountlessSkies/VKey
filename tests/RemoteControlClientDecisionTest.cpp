// Tests for the remote-control client passthrough decision.
// SPDX-License-Identifier: GPL-3.0-only

#include <gtest/gtest.h>

#include "core/RemoteControlClientDecision.h"

using NextKey::IsRemoteControlClientExe;

TEST(RemoteControlClientDecision, RustDeskIsPassedThrough) {
    EXPECT_TRUE(IsRemoteControlClientExe(L"rustdesk.exe"));
}

TEST(RemoteControlClientDecision, WindowsRemoteDesktopKeepsLocalTyping) {
    // mstsc forwards Unicode; users type Vietnamese through it into machines
    // without an IME, so the local VKey must keep composing there.
    EXPECT_FALSE(IsRemoteControlClientExe(L"mstsc.exe"));
}

TEST(RemoteControlClientDecision, OrdinaryAppsUnaffected) {
    EXPECT_FALSE(IsRemoteControlClientExe(L"notepad.exe"));
    EXPECT_FALSE(IsRemoteControlClientExe(L""));
}

TEST(RemoteControlClientDecision, MatchIsExact) {
    // exeName is already lower-cased by FocusOwner; no partial matches.
    EXPECT_FALSE(IsRemoteControlClientExe(L"rustdesk.exe.bak"));
    EXPECT_FALSE(IsRemoteControlClientExe(L"myrustdesk.exe"));
}
