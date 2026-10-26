#pragma once

#include <cstddef>
#include <string>

namespace regexforge {
struct RuntimeMetrics {
    std::size_t backtracking_steps = 0;
    std::size_t active_nfa_states = 0;
    std::size_t generated_dfa_states = 0;
    std::size_t elapsed_microseconds = 0;
};

struct RuntimeLimits {
    std::size_t max_backtracking_steps = 1'000'000;
    std::size_t max_active_nfa_states = 1'000'000;
    std::size_t max_generated_dfa_states = 100'000;
    std::size_t max_elapsed_microseconds = 0;
};

class RuntimeMonitor {
public:
    explicit RuntimeMonitor(RuntimeLimits limits = {});
    void reset();
    void record_backtracking_steps(std::size_t steps);
    void record_active_nfa_states(std::size_t states);
    void record_generated_dfa_states(std::size_t states);
    void record_elapsed_microseconds(std::size_t elapsed);
    bool exceeded() const noexcept;
    std::string violation() const;
    const RuntimeMetrics& metrics() const noexcept { return metrics_; }

private:
    RuntimeLimits limits_;
    RuntimeMetrics metrics_;
};
}  // namespace regexforge
