#pragma once

#include "regexforge/ast.h"

#include <cstddef>

namespace regexforge {

struct AnalyzerConfig {
    std::size_t input_length_cap = 256;
    std::size_t nfa_state_cap = 128;
    std::size_t ambiguity_cap = 32;
    std::size_t quantifier_depth_cap = 8;
};

struct AstFeatures {
    std::size_t node_count = 0;
    std::size_t estimated_nfa_states = 0;
    std::size_t nested_quantifiers = 0;
    std::size_t overlapping_alternations = 0;
    std::size_t ambiguous_paths = 0;
    std::size_t quantifier_depth = 0;
    std::size_t input_length = 0;
};

struct NormalizedFeatures {
    double nested_quantifiers = 0.0;
    double overlapping_alternations = 0.0;
    double ambiguous_paths = 0.0;
    double quantifier_depth = 0.0;
    double estimated_nfa_states = 0.0;
    double input_length = 0.0;
};

struct RiskWeights {
    double nested_quantifiers = 0.25;
    double overlapping_alternations = 0.20;
    double ambiguous_paths = 0.20;
    double quantifier_depth = 0.15;
    double estimated_nfa_states = 0.10;
    double input_length = 0.10;
};

struct AnalysisResult {
    AstFeatures features;
    NormalizedFeatures normalized;
    double risk_score = 0.0;
};

class PredictiveAnalyzer {
public:
    explicit PredictiveAnalyzer(AnalyzerConfig config = {}, RiskWeights weights = {});
    AnalysisResult analyze(const Ast& ast, std::size_t input_length) const;

private:
    AnalyzerConfig config_;
    RiskWeights weights_;
};

}  // namespace regexforge
