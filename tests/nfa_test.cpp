#include "regexforge/nfa.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

using namespace regexforge;

namespace {
bool matches(const std::string& pattern, const std::string& input) {
    ThompsonNFAMatcher matcher(parse_pattern(pattern));
    return matcher.full_match(input).matched;
}
}  // namespace

TEST(NfaTest, PerformsFullStringMatching) {
    EXPECT_TRUE(matches("(a|b)*abb", "abababb"));
    EXPECT_FALSE(matches("ab*", "ac"));
    EXPECT_TRUE(matches("a|bc*", "bcc"));
}

TEST(NfaTest, HandlesEmptyExpressionsAndStars) {
    EXPECT_TRUE(matches("", ""));
    EXPECT_FALSE(matches("", "a"));
    EXPECT_TRUE(matches("()*", ""));
    EXPECT_FALSE(matches("()*", "a"));
}

TEST(NfaTest, ReportsActiveStateMetric) {
    ThompsonNFAMatcher matcher(parse_pattern("(a|aa)*b"));
    EXPECT_FALSE(matcher.full_match("aaaa").matched);
    EXPECT_GT(matcher.full_match("aaaa").metrics.active_nfa_states, 0U);
    EXPECT_GT(matcher.state_count(), 0U);
}
