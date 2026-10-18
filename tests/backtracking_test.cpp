#include "regexforge/backtracking.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

using namespace regexforge;

namespace {
bool matches(const std::string& pattern, const std::string& input, BacktrackingLimits limits = {}) {
    BacktrackingMatcher matcher(parse_pattern(pattern), limits);
    return matcher.full_match(input);
}
}

TEST(BacktrackingTest, PerformsFullStringMatching) {
    EXPECT_TRUE(matches("(a|b)*abb", "abababb"));
    EXPECT_FALSE(matches("ab*", "ac"));
    EXPECT_TRUE(matches("a|bc*", "bcc"));
    EXPECT_TRUE(matches("", ""));
    EXPECT_FALSE(matches("", "a"));
}

TEST(BacktrackingTest, HandlesEmptyStarWithoutInfiniteRecursion) {
    EXPECT_TRUE(matches("()*", ""));
    EXPECT_FALSE(matches("()*", "a"));
}

TEST(BacktrackingTest, EnforcesExecutionLimit) {
    BacktrackingMatcher matcher(parse_pattern("(a|aa)*b"), BacktrackingLimits{100});
    EXPECT_THROW(matcher.full_match("aaaaaaaaaaaaaaaa"), ExecutionLimitExceeded);
    EXPECT_EQ(matcher.metrics().steps, 100U);
}

TEST(BacktrackingTest, ExposesAmbiguousWorkGrowth) {
    BacktrackingMatcher short_match(parse_pattern("(a|aa)*b"));
    BacktrackingMatcher longer_match(parse_pattern("(a|aa)*b"));
    EXPECT_FALSE(short_match.full_match("aaaaa"));
    EXPECT_FALSE(longer_match.full_match("aaaaaaaaa"));
    EXPECT_GT(longer_match.metrics().steps, short_match.metrics().steps);
}
