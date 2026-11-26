// Tests for the "Vietnamese only inside the game's text boxes" decisions.
// SPDX-License-Identifier: GPL-3.0-only

#include <gtest/gtest.h>

#include "core/GameChatGateDecision.h"

using NextKey::DecodeGameTextFocus;
using NextKey::ShouldPassThroughForGameChat;

// --- DecodeGameTextFocus -----------------------------------------------------

TEST(GameChatGateDecision, NativeBitMeansTextBoxFocused) {
    // League of Legends reports 0x1 while chat / shop search has focus.
    EXPECT_TRUE(DecodeGameTextFocus(true, 0x1));
}

TEST(GameChatGateDecision, ZeroMeansPlaying) {
    // ...and 0 while the player is fighting.
    EXPECT_FALSE(DecodeGameTextFocus(true, 0x0));
}

TEST(GameChatGateDecision, OtherBitsWithNativeStillFocused) {
    EXPECT_TRUE(DecodeGameTextFocus(true, 0x1 | 0x8 | 0x10));
}

TEST(GameChatGateDecision, OtherBitsWithoutNativeNotFocused) {
    EXPECT_FALSE(DecodeGameTextFocus(true, 0x8 | 0x10));
}

TEST(GameChatGateDecision, FailedQueryFailsSafeToPlaying) {
    // No IME window / SendMessageTimeout failed (probe logged -1 / -2 while
    // alt-tabbing). Whatever the stale value, treat as "no text box".
    EXPECT_FALSE(DecodeGameTextFocus(false, 0x1));
    EXPECT_FALSE(DecodeGameTextFocus(false, -1));
    EXPECT_FALSE(DecodeGameTextFocus(false, -2));
}

// --- GameTextFocusFilter -----------------------------------------------------

using NextKey::GameTextFocusFilter;

TEST(GameTextFocusFilter, FollowsSuccessfulProbes) {
    GameTextFocusFilter f;
    EXPECT_FALSE(f.Update(true, 0x0, 0));
    EXPECT_TRUE(f.Update(true, 0x1, 15));
    EXPECT_FALSE(f.Update(true, 0x0, 30));
}

TEST(GameTextFocusFilter, ShortTimeoutWhileChattingKeepsGateOpen) {
    // Pattern from the log: ok=1 conv=1, then a single ERROR_TIMEOUT tick.
    GameTextFocusFilter f;
    EXPECT_TRUE(f.Update(true, 0x1, 0));
    EXPECT_TRUE(f.Update(false, -2, 15));
    EXPECT_TRUE(f.Update(false, -2, 30));
    EXPECT_TRUE(f.Update(true, 0x1, 45));
}

TEST(GameTextFocusFilter, ShortTimeoutWhilePlayingStaysClosed) {
    GameTextFocusFilter f;
    EXPECT_FALSE(f.Update(true, 0x0, 0));
    EXPECT_FALSE(f.Update(false, -2, 15));
}

TEST(GameTextFocusFilter, LongFailureFallsBackToClosed) {
    GameTextFocusFilter f;
    EXPECT_TRUE(f.Update(true, 0x1, 1000));
    EXPECT_TRUE(f.Update(false, -2, 1015));
    EXPECT_TRUE(f.Update(false, -2, 1000 + GameTextFocusFilter::kFailHoldMs - 1));
    EXPECT_FALSE(f.Update(false, -2, 1000 + GameTextFocusFilter::kFailHoldMs + 15));
}

TEST(GameTextFocusFilter, FailureWindowRestartsAfterSuccess) {
    GameTextFocusFilter f;
    EXPECT_TRUE(f.Update(true, 0x1, 0));
    EXPECT_TRUE(f.Update(false, -2, 150));   // failing since 150
    EXPECT_TRUE(f.Update(true, 0x1, 300));   // recovered
    EXPECT_TRUE(f.Update(false, -2, 450));   // new streak starts at 450
    EXPECT_TRUE(f.Update(false, -2, 600));   // only 150 ms into it
}

TEST(GameTextFocusFilter, ResetForgetsOpenGate) {
    GameTextFocusFilter f;
    EXPECT_TRUE(f.Update(true, 0x1, 0));
    f.Reset();
    EXPECT_FALSE(f.Update(false, -2, 15));
}

// --- ShouldPassThroughForGameChat -------------------------------------------

TEST(GameChatGateDecision, NormalAppNeverPassesThrough) {
    EXPECT_FALSE(ShouldPassThroughForGameChat(false, false));
    EXPECT_FALSE(ShouldPassThroughForGameChat(false, true));
}

TEST(GameChatGateDecision, GatedAppWhilePlayingPassesThrough) {
    EXPECT_TRUE(ShouldPassThroughForGameChat(true, false));
}

TEST(GameChatGateDecision, GatedAppInChatTypesNormally) {
    EXPECT_FALSE(ShouldPassThroughForGameChat(true, true));
}

// Sequence recorded in League of Legends Practice Tool (probe log 2026-11-26):
// playing -> Enter (chat) -> Enter (send) -> Enter -> Esc -> Enter -> right
// click on the map. The gate must follow the game's flag, including the mouse
// close that Enter-counting would miss.
TEST(GameChatGateDecision, RecordedLeagueSequence) {
    struct Step { std::int64_t conv; bool expectPass; };
    const Step steps[] = {
        {0x0, true},   // fighting
        {0x1, false},  // Enter opens chat
        {0x0, true},   // Enter sends
        {0x1, false},  // Shift+Enter all-chat
        {0x0, true},   // Esc cancels
        {0x1, false},  // Enter opens chat
        {0x0, true},   // right click on the map closes it (no key event)
        {0x1, false},  // click into shop search box
        {0x0, true},   // click away
    };
    for (const auto& s : steps) {
        const bool focused = DecodeGameTextFocus(true, s.conv);
        EXPECT_EQ(ShouldPassThroughForGameChat(true, focused), s.expectPass)
            << "conv=" << s.conv;
    }
}
