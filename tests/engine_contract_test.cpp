#include "regexforge/backtracking.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

using namespace regexforge;

TEST(EngineContractTest, BacktrackingImplementsSharedMatcherContract) {
    std::unique_ptr<Matcher> matcher = std::make_unique<BacktrackingMatcher>(parse_pattern("ab*"));
    const MatchResult result = matcher->full_match("abbb");
    EXPECT_TRUE(result.matched);
    EXPECT_GT(result.metrics.backtracking_steps, 0U);
}
