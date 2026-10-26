#include "regexforge/analyzer.h"

#include <algorithm>

namespace regexforge {
namespace {
struct Summary {
    bool nullable = false;
    bool has_literal = false;
    char first_literal = 0;
    std::size_t node_count = 0;
    std::size_t estimated_nfa_states = 0;
    std::size_t nested_quantifiers = 0;
    std::size_t overlapping_alternations = 0;
    std::size_t ambiguous_paths = 0;
    std::size_t max_quantifier_depth = 0;
};

Summary visit(const Ast& node, std::size_t depth) {
    Summary result{false, false, 0, 1, 1, 0, 0, 0, 0};
    switch (node->kind) {
        case AstNode::Kind::Empty: result.nullable = true; return result;
        case AstNode::Kind::Literal: result.has_literal = true; result.first_literal = node->literal; return result;
        case AstNode::Kind::Star: {
            result.nullable = true;
            result.max_quantifier_depth = depth + 1;
            if (depth > 0) ++result.nested_quantifiers;
            const Summary child = visit(node->children.front(), depth + 1);
            result.has_literal = child.has_literal;
            result.first_literal = child.first_literal;
            result.node_count += child.node_count;
            result.estimated_nfa_states += child.estimated_nfa_states + 1;
            result.nested_quantifiers += child.nested_quantifiers;
            result.overlapping_alternations += child.overlapping_alternations;
            result.ambiguous_paths += child.ambiguous_paths;
            result.max_quantifier_depth = std::max(result.max_quantifier_depth, child.max_quantifier_depth);
            return result;
        }
        case AstNode::Kind::Concatenation: {
            result.nullable = true;
            for (const auto& child_node : node->children) {
                const Summary child = visit(child_node, depth);
                result.nullable = result.nullable && child.nullable;
                if (!result.has_literal && child.has_literal) {
                    result.has_literal = true;
                    result.first_literal = child.first_literal;
                }
                result.node_count += child.node_count;
                result.estimated_nfa_states += child.estimated_nfa_states;
                result.nested_quantifiers += child.nested_quantifiers;
                result.overlapping_alternations += child.overlapping_alternations;
                result.ambiguous_paths += child.ambiguous_paths;
                result.max_quantifier_depth = std::max(result.max_quantifier_depth, child.max_quantifier_depth);
            }
            return result;
        }
        case AstNode::Kind::Alternation: {
            result.ambiguous_paths += node->children.size() > 1 ? node->children.size() - 1 : 0;
            for (std::size_t i = 0; i < node->children.size(); ++i) {
                const Summary child = visit(node->children[i], depth);
                result.nullable = result.nullable || child.nullable;
                if (!result.has_literal && child.has_literal) {
                    result.has_literal = true;
                    result.first_literal = child.first_literal;
                }
                for (std::size_t j = 0; j < i; ++j) {
                    const Summary prior = visit(node->children[j], depth);
                    if ((child.has_literal && prior.has_literal && child.first_literal == prior.first_literal) ||
                        child.nullable || prior.nullable) {
                        ++result.overlapping_alternations;
                        break;
                    }
                }
                result.node_count += child.node_count;
                result.estimated_nfa_states += child.estimated_nfa_states;
                result.nested_quantifiers += child.nested_quantifiers;
                result.overlapping_alternations += child.overlapping_alternations;
                result.ambiguous_paths += child.ambiguous_paths;
                result.max_quantifier_depth = std::max(result.max_quantifier_depth, child.max_quantifier_depth);
            }
            return result;
        }
    }
    return result;
}

double normalize(std::size_t value, std::size_t cap) {
    return cap == 0 ? 0.0 : std::min(1.0, static_cast<double>(value) / static_cast<double>(cap));
}
}  // namespace

PredictiveAnalyzer::PredictiveAnalyzer(AnalyzerConfig config, RiskWeights weights)
    : config_(config), weights_(weights) {}

AnalysisResult PredictiveAnalyzer::analyze(const Ast& ast, std::size_t input_length) const {
    const Summary summary = visit(ast, 0);
    AnalysisResult result;
    result.features = {summary.node_count, summary.estimated_nfa_states, summary.nested_quantifiers,
                       summary.overlapping_alternations, summary.ambiguous_paths,
                       summary.max_quantifier_depth, input_length};
    result.normalized = {normalize(summary.nested_quantifiers, config_.ambiguity_cap),
                         normalize(summary.overlapping_alternations, config_.ambiguity_cap),
                         normalize(summary.ambiguous_paths, config_.ambiguity_cap),
                         normalize(summary.max_quantifier_depth, config_.quantifier_depth_cap),
                         normalize(summary.estimated_nfa_states, config_.nfa_state_cap),
                         normalize(input_length, config_.input_length_cap)};
    result.risk_score = 100.0 * (weights_.nested_quantifiers * result.normalized.nested_quantifiers +
                                 weights_.overlapping_alternations * result.normalized.overlapping_alternations +
                                 weights_.ambiguous_paths * result.normalized.ambiguous_paths +
                                 weights_.quantifier_depth * result.normalized.quantifier_depth +
                                 weights_.estimated_nfa_states * result.normalized.estimated_nfa_states +
                                 weights_.input_length * result.normalized.input_length);
    return result;
}
}  // namespace regexforge
