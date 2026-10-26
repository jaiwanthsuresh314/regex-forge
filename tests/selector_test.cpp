#include "regexforge/parser.h"
#include "regexforge/runtime_monitor.h"
#include "regexforge/strategy_selector.h"

#include <gtest/gtest.h>

using namespace regexforge;

TEST(SelectorTest, ExplainsStrategyChoice) {
    StrategySelector selector;
    const auto simple = selector.select(parse_pattern("abc"), {3, 1, 100, true});
    EXPECT_EQ(simple.strategy, Strategy::Backtracking);
    EXPECT_NE(simple.reason.find("risk="), std::string::npos);

    const auto risky = selector.select(parse_pattern("(a|aa)*b"), {128, 8, 1000, true});
    EXPECT_EQ(risky.strategy, Strategy::Backtracking);

    StrategyThresholds dfa_thresholds;
    dfa_thresholds.nfa_risk = 5.0;
    dfa_thresholds.dfa_risk = 10.0;
    const auto repeated = StrategySelector(PredictiveAnalyzer{}, dfa_thresholds)
                              .select(parse_pattern("(a|aa)*b"), {128, 8, 1000, true});
    EXPECT_EQ(repeated.strategy, Strategy::LazyDFA);
}

TEST(RuntimeMonitorTest, ReportsTheFirstLimitViolation) {
    RuntimeMonitor monitor(RuntimeLimits{10, 20, 30, 40});
    monitor.record_backtracking_steps(11);
    monitor.record_active_nfa_states(25);
    EXPECT_TRUE(monitor.exceeded());
    EXPECT_EQ(monitor.violation(), "backtracking step limit exceeded");
    monitor.reset();
    EXPECT_FALSE(monitor.exceeded());
}
