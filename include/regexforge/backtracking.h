#pragma once

#include "regexforge/ast.h"

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace regexforge {

struct BacktrackingLimits {
    std::size_t max_steps = 1'000'000;
};

struct BacktrackingMetrics {
    std::size_t steps = 0;
};

class ExecutionLimitExceeded : public std::runtime_error {
public:
    explicit ExecutionLimitExceeded(std::size_t limit);
    std::size_t limit() const noexcept { return limit_; }

private:
    std::size_t limit_;
};

class BacktrackingMatcher {
public:
    explicit BacktrackingMatcher(Ast ast, BacktrackingLimits limits = {});

    bool full_match(const std::string& input);
    const BacktrackingMetrics& metrics() const noexcept { return metrics_; }

private:
    using Continuation = std::function<bool(std::size_t)>;

    bool match_node(const Ast& node, const std::string& input, std::size_t position, const Continuation& continuation);
    bool match_sequence(const std::vector<Ast>& nodes, std::size_t index, const std::string& input,
                        std::size_t position, const Continuation& continuation);
    bool match_star(const Ast& child, const std::string& input, std::size_t position,
                    const Continuation& continuation);
    void count_step();

    Ast ast_;
    BacktrackingLimits limits_;
    BacktrackingMetrics metrics_;
};

}  // namespace regexforge
