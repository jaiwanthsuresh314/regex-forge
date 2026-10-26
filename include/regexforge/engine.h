#pragma once

#include <cstddef>
#include <string_view>

namespace regexforge {

struct ExecutionMetrics {
    std::size_t backtracking_steps = 0;
    std::size_t active_nfa_states = 0;
    std::size_t generated_dfa_states = 0;
    std::size_t elapsed_microseconds = 0;
};

struct MatchResult {
    bool matched = false;
    ExecutionMetrics metrics;
};

class Matcher {
public:
    virtual MatchResult full_match(std::string_view input) = 0;
    virtual ~Matcher() = default;
};

}  // namespace regexforge
