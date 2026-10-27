#include "regexforge/backtracking.h"
#include "regexforge/dispatcher.h"
#include "regexforge/nfa.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

#include <memory>
#include <string_view>

using namespace regexforge;

TEST(DispatcherTest, SelectsAndRunsTheRequestedEngine) {
    const Ast ast = parse_pattern("(a|b)*abb");
    EngineDispatcher dispatcher;
    const auto result = dispatcher.full_match(
        ast, "abababb", WorkloadCharacteristics{7, 1, 100, true},
        [&](Strategy strategy) -> std::unique_ptr<Matcher> {
            if (strategy == Strategy::Backtracking) return std::make_unique<BacktrackingMatcher>(ast);
            return std::make_unique<ThompsonNFAMatcher>(ast);
        });

    EXPECT_EQ(result.strategy, Strategy::Backtracking);
    EXPECT_TRUE(result.match.matched);
    EXPECT_EQ(result.selection.strategy, result.strategy);
}

TEST(DispatcherTest, ConformanceAcrossAvailableEngines) {
    const Ast ast = parse_pattern("(a|aa)*b");
    for (const auto& input : {"b", "aaab", "aaaa", "c"}) {
        BacktrackingMatcher backtracking(ast);
        ThompsonNFAMatcher nfa(ast);
        EXPECT_EQ(backtracking.full_match(std::string_view(input)).matched,
                  nfa.full_match(std::string_view(input)).matched);
    }
}

TEST(DispatcherTest, RejectsInvalidFactories) {
    EngineDispatcher dispatcher;
    EXPECT_THROW(dispatcher.full_match(parse_pattern("a"), "a", {}, {}), std::invalid_argument);
    EXPECT_THROW(dispatcher.full_match(parse_pattern("a"), "a", {},
                                       [](Strategy) { return std::unique_ptr<Matcher>{}; }),
                 std::invalid_argument);
}
