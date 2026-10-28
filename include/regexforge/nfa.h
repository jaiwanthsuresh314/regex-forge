#pragma once

#include "regexforge/ast.h"
#include "regexforge/engine.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace regexforge {

using StateId = std::size_t;

struct EpsilonTransition {
    StateId to{0};

    constexpr bool operator==(const EpsilonTransition&) const = default;
};

struct CharTransition {
    char symbol{'\0'};
    StateId to{0};

    constexpr bool operator==(const CharTransition&) const = default;
};

struct NfaState {
    StateId id{0};
    std::vector<EpsilonTransition> epsilon_transitions;
    std::vector<CharTransition> char_transitions;
    bool is_accept{false};

    void add_epsilon(StateId target) {
        epsilon_transitions.push_back(EpsilonTransition{target});
    }

    void add_transition(char symbol, StateId target) {
        char_transitions.push_back(CharTransition{symbol, target});
    }
};

struct NfaFragment {
    StateId start{0};
    StateId accept{0};
};

struct NfaMetrics : public ExecutionMetrics {
    std::size_t active_states{0};
    std::vector<std::size_t> active_state_counts;
};

class ThompsonNfa {
public:
    ThompsonNfa() = default;
    explicit ThompsonNfa(const Ast& ast);

    static ThompsonNfa from_ast(const Ast& ast);

    StateId add_state();
    void add_epsilon_transition(StateId from, StateId to);
    void add_char_transition(StateId from, char symbol, StateId to);
    void set_start_state(StateId id) noexcept { start_state_ = id; }
    void set_accept_state(StateId id) noexcept;
    void set_accept(StateId id, bool accept = true);

    StateId start_state() const noexcept { return start_state_; }
    StateId accept_state() const noexcept { return accept_state_; }
    std::size_t num_states() const noexcept { return states_.size(); }
    const std::vector<NfaState>& states() const noexcept { return states_; }
    const NfaState& state(StateId id) const { return states_.at(id); }
    NfaState& state(StateId id) { return states_.at(id); }

    std::vector<StateId> epsilon_closure(StateId state) const;
    std::vector<StateId> epsilon_closure(const std::vector<StateId>& states) const;

    std::vector<StateId> step(const std::vector<StateId>& current_states, char symbol) const;

    MatchResult simulate(std::string_view input, NfaMetrics* detailed_metrics = nullptr,
                         bool early_termination = true) const;

private:
    std::vector<NfaState> states_;
    StateId start_state_{0};
    StateId accept_state_{0};
};

ThompsonNfa build_thompson_nfa(const Ast& ast);

struct NfaLimits {
    std::size_t max_active_states = 1'000'000;
};

class ThompsonNFAMatcher : public Matcher {
public:
    explicit ThompsonNFAMatcher(Ast ast, NfaLimits limits = {});
    explicit ThompsonNFAMatcher(ThompsonNfa nfa, NfaLimits limits = {});

    MatchResult full_match(std::string_view input) override;

    std::size_t state_count() const noexcept { return nfa_.num_states(); }
    const ThompsonNfa& nfa() const noexcept { return nfa_; }
    const NfaMetrics& metrics() const noexcept { return metrics_; }

private:
    ThompsonNfa nfa_;
    NfaLimits limits_;
    NfaMetrics metrics_{};
};

using NfaMatcher = ThompsonNFAMatcher;
using ThompsonNfaMatcher = ThompsonNFAMatcher;

}  // namespace regexforge
