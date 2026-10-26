#include "regexforge/analyzer.h"
#include "regexforge/parser.h"

#include <gtest/gtest.h>

using namespace regexforge;

TEST(AnalyzerTest, ExtractsStructuralRiskFeatures) {
    const auto result = PredictiveAnalyzer().analyze(parse_pattern("(a|aa)*b"), 64);
    EXPECT_GT(result.features.overlapping_alternations, 0U);
    EXPECT_GT(result.features.ambiguous_paths, 0U);
    EXPECT_GT(result.features.estimated_nfa_states, 0U);
    EXPECT_GT(result.risk_score, 0.0);
    EXPECT_LE(result.risk_score, 100.0);
}

TEST(AnalyzerTest, NormalizationAndWeightsAreConfigurable) {
    AnalyzerConfig config;
    config.input_length_cap = 10;
    RiskWeights weights{0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    const auto result = PredictiveAnalyzer(config, weights).analyze(parse_pattern("a"), 20);
    EXPECT_DOUBLE_EQ(result.normalized.input_length, 1.0);
    EXPECT_DOUBLE_EQ(result.risk_score, 100.0);
}
