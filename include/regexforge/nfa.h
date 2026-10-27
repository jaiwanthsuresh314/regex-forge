#pragma once

#include "regexforge/ast.h"
#include "regexforge/engine.h"

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace regexforge {

struct NfaLimits {
    std::size_t max_active_states = 1'000'000;
};

class ThompsonNFAMatcher : public Matcher {
public:
    explicit ThompsonNFAMatcher(Ast ast, NfaLimits limits = {});

    MatchResult full_match(std::string_view input) override;
    std::size_t state_count() const noexcept { return states_.size(); }

private:
    struct State {
        std::vector<std::size_t> epsilon;
        std::vector<std::pair<char, std::size_t>> transitions;
        bool accepting = false;
    };
    struct Fragment { std::size_t start; std::size_t end; };

    Fragment compile(const Ast& node);
    std::size_t add_state();
    void epsilon_closure(std::vector<std::size_t>& states) const;
    void record_active(std::size_t count);

    Ast ast_;
    NfaLimits limits_;
    std::vector<State> states_;
    std::size_t start_ = 0;
    std::size_t accept_ = 0;
    std::size_t max_active_states_ = 0;
};

}  // namespace regexforge
