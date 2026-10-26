#include "regexforge/runtime_adaptation.h"

#include <gtest/gtest.h>

using namespace regexforge;

TEST(RuntimeAdaptationTest, ContinuesWithinLimits) {
    RuntimeAdapter adapter;
    RuntimeMonitor monitor;
    const auto decision = adapter.decide(Strategy::Backtracking, monitor);
    EXPECT_EQ(decision.action, AdaptationAction::Continue);
    EXPECT_FALSE(decision.restart_from_input);
}

TEST(RuntimeAdaptationTest, RestartsBacktrackingWithNfa) {
    RuntimeMonitor monitor(RuntimeLimits{10, 100, 100, 0});
    monitor.record_backtracking_steps(11);
    const auto decision = RuntimeAdapter().decide(Strategy::Backtracking, monitor);
    EXPECT_EQ(decision.action, AdaptationAction::RestartWithThompsonNFA);
    EXPECT_TRUE(decision.restart_from_input);
    EXPECT_EQ(decision.target_strategy, Strategy::ThompsonNFA);
}

TEST(RuntimeAdaptationTest, AbortsWhenNoFallbackIsSafe) {
    RuntimeMonitor monitor(RuntimeLimits{100, 100, 2, 0});
    monitor.record_generated_dfa_states(3);
    const auto decision = RuntimeAdapter(AdaptationPolicy{false, false, true})
                              .decide(Strategy::LazyDFA, monitor);
    EXPECT_EQ(decision.action, AdaptationAction::Abort);
    EXPECT_NE(decision.reason.find("no safe fallback"), std::string::npos);
}
