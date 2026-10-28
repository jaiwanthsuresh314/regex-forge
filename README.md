# Regex Forge

Regex Forge is a C++20 experimental framework for workload-aware regex execution. Milestones 1 and 2 provide the shared front-end foundation and a bounded baseline backtracking matcher.

## Supported grammar

```text
expression    := concatenation ('|' concatenation)*
concatenation := repetition*
repetition    := atom ('*')*
atom          := LITERAL | '(' expression ')'
```

The implementation permits at most one `*` after an atom; a second consecutive `*` is rejected. Ordinary characters are single-character literals. There are no escapes, character classes, Unicode-specific rules, anchors, or newline special cases yet.

An empty pattern matches the empty string. Because concatenation may contain zero repetitions, empty alternatives (`|a`, `a|`, and `|`) and empty groups (`()`) are valid and represented by an explicit `Empty` AST node. The backtracking matcher performs full-string matching.

## Progress

### Milestone 1 — complete

- Lexer and token positions.
- Shared immutable AST and debug printer.
- Recursive-descent parser with precedence, grouping, empty-string semantics, and useful zero-based diagnostics.
- GoogleTest unit tests for lexing, parsing, precedence, empty alternatives, and malformed patterns.

### Milestone 2 — complete

The baseline `BacktrackingMatcher` executes the shared AST using greedy depth-first backtracking. It supports literals, concatenation, alternation, empty nodes, and `*`, while preserving full-string semantics. Each attempted AST node increments a step counter. A configurable step limit throws `ExecutionLimitExceeded`, preventing unbounded pathological execution. Empty repetitions are detected so patterns such as `()*` terminate safely.

The matcher is intentionally a correctness baseline, not a production-safe general-purpose engine. Its metrics expose the ambiguous-work growth that later predictive selection and runtime monitoring will address.

### Milestone 3 — Thompson NFA — complete

The first alternative engine is now available as `ThompsonNFAMatcher`. It compiles the shared AST into epsilon-NFA fragments, simulates epsilon closure for each input character, performs full-string matching, and reports the maximum active-state count through the shared `ExecutionMetrics` contract. An active-state limit is available for bounded execution.

### Workflow 3 — predictive execution layer — in progress

The current implementation adds an engine-independent predictive policy layer:

- `PredictiveAnalyzer` extracts AST structure and workload features.
- Features are normalized against configurable caps and combined with configurable weights using the proposal's weighted risk-score baseline.
- `StrategySelector` chooses backtracking, Thompson NFA, or lazy DFA based on risk, input size, expected repetition count, and DFA resource permission.
- Every selection includes the analysis result and a human-readable reason.
- `RuntimeMonitor` records backtracking steps, active NFA states, generated DFA states, and elapsed time against configurable limits.
- `RuntimeAdapter` converts a limit violation into an explicit action: continue, restart with Thompson NFA, restart with lazy DFA, or abort.
- `EngineDispatcher` connects a selection decision to an engine factory without coupling the policy layer to engine internals.
- `Matcher`, `MatchResult`, and `ExecutionMetrics` define the shared engine contract; the backtracking matcher now implements it while retaining a boolean compatibility helper.

The dispatcher currently performs the initial selection and full match through a caller-provided engine factory. Runtime adaptation remains restart-from-input: no mid-match state transfer is claimed safe across engines. The caller must instantiate the target engine and restart matching after receiving a restart decision.

### Milestone 4 — Lazy DFA — complete for the current AST

`LazyDFAMatcher` builds a private Thompson-style NFA and constructs DFA states on demand, caching them across `full_match` calls. It supports the current AST grammar, reports generated-state and elapsed-time metrics, and falls back to Thompson NFA execution when the configured state or memory limit is reached. After the configured number of fallback events, further limit violations throw instead of growing without bound.

## Build and test

GoogleTest must be available to CMake (for example, through a package configuration visible to `find_package(GTest CONFIG)`). The test target is omitted with a warning when GoogleTest is unavailable.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

## Current limitations

The parser accepts every non-metacharacter as a literal and reports syntax errors with a zero-based source position. Character classes, Unicode-specific rules, anchors, backreferences, lookarounds, and general min/max quantifiers are not yet part of the AST. The current backtracking matcher has no recursion-depth limit separate from its step limit.

## Next steps

Milestones 3 and 4 should add Thompson NFA and lazy DFA engines. Workflow 3 is implemented independently of those engines: it can analyze patterns and produce explainable strategy decisions now, while actual engine dispatch will be integrated once those engines expose the shared matcher contract.

## Independent development workflows

The remaining work is divided into three independent vertical workflows. Integration and merging are intentionally kept separate from these assignments.

### Workflow 1 — Thompson NFA

Owner: Collaborator 1

- Implement NFA states and epsilon transitions.
- Build a Thompson NFA from the shared AST.
- Implement epsilon closure and NFA simulation.
- Track active states and simulation steps.
- Add NFA-specific correctness tests.

Suggested files:

```text
include/regexforge/nfa.h
src/nfa.cpp
tests/nfa_test.cpp
```

### Workflow 2 — Lazy DFA — complete

Owner: Collaborator 2

- Implement on-demand DFA state construction.
- Cache DFA states keyed by NFA state sets.
- Add configurable state and memory limits.
- Define fallback behavior when limits are reached.
- Track generated states, cache hits, and construction cost.
- Add DFA-specific tests.

Suggested files:

```text
include/regexforge/lazy_dfa.h
src/lazy_dfa.cpp
tests/lazy_dfa_test.cpp
```

The implementation uses a private NFA representation rather than depending on internal classes from the Thompson NFA workflow. Both engines expose compatible matching results and metrics.

### Workflow 3 — Predictive execution layer

Owner: Project lead

- Extract AST features for nested quantifiers, overlapping alternations, ambiguity, quantifier depth, estimated NFA states, and input length.
- Implement configurable normalized risk scoring.
- Select among backtracking, Thompson NFA, and lazy DFA strategies.
- Record explainable selection decisions and thresholds.
- Define runtime monitoring interfaces.
- Design safe switching or restart-based fallback behavior.
- Add benchmark and evaluation scaffolding.

Suggested files:

```text
include/regexforge/analyzer.h
include/regexforge/strategy_selector.h
include/regexforge/runtime_monitor.h
src/analyzer.cpp
src/strategy_selector.cpp
src/runtime_monitor.cpp
tests/analyzer_test.cpp
tests/selector_test.cpp
benchmarks/
docs/
```

## Shared engine contract

Before parallel implementation, agree on a small public result and matcher contract. Internal NFA and DFA data structures should remain workflow-local.

```cpp
struct MatchResult {
    bool matched;
    ExecutionMetrics metrics;
};

class Matcher {
public:
    virtual MatchResult full_match(std::string_view input) = 0;
    virtual ~Matcher() = default;
};
```

Each workflow should avoid modifying the parser, AST, or existing backtracking engine. Shared changes should be additive interfaces, build configuration, or documentation updates.
