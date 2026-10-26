#include "regexforge/runtime_monitor.h"

namespace regexforge {
RuntimeMonitor::RuntimeMonitor(RuntimeLimits limits) : limits_(limits) {}
void RuntimeMonitor::reset() { metrics_ = {}; }
void RuntimeMonitor::record_backtracking_steps(std::size_t steps) { metrics_.backtracking_steps = steps; }
void RuntimeMonitor::record_active_nfa_states(std::size_t states) { metrics_.active_nfa_states = states; }
void RuntimeMonitor::record_generated_dfa_states(std::size_t states) { metrics_.generated_dfa_states = states; }
void RuntimeMonitor::record_elapsed_microseconds(std::size_t elapsed) { metrics_.elapsed_microseconds = elapsed; }
bool RuntimeMonitor::exceeded() const noexcept {
    return metrics_.backtracking_steps > limits_.max_backtracking_steps ||
           metrics_.active_nfa_states > limits_.max_active_nfa_states ||
           metrics_.generated_dfa_states > limits_.max_generated_dfa_states ||
           (limits_.max_elapsed_microseconds > 0 && metrics_.elapsed_microseconds > limits_.max_elapsed_microseconds);
}
std::string RuntimeMonitor::violation() const {
    if (metrics_.backtracking_steps > limits_.max_backtracking_steps) return "backtracking step limit exceeded";
    if (metrics_.active_nfa_states > limits_.max_active_nfa_states) return "active NFA state limit exceeded";
    if (metrics_.generated_dfa_states > limits_.max_generated_dfa_states) return "generated DFA state limit exceeded";
    if (limits_.max_elapsed_microseconds > 0 && metrics_.elapsed_microseconds > limits_.max_elapsed_microseconds)
        return "elapsed time limit exceeded";
    return {};
}
}  // namespace regexforge
