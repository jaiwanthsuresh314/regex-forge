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

## Build and test

GoogleTest must be available to CMake (for example, through a package configuration visible to `find_package(GTest CONFIG)`). The test target is omitted with a warning when GoogleTest is unavailable.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

## Current limitations

There is currently no Thompson NFA, lazy DFA, predictive analyzer, runtime monitor, strategy selector, or runtime switching. The parser accepts every non-metacharacter as a literal and reports syntax errors with a zero-based source position. The current matcher has no recursion-depth limit separate from its step limit.

## Next steps

Milestone 3 should add Thompson NFA construction and simulation over the same AST, with active-state metrics and differential tests against the backtracking baseline. After that, the predictive analyzer can compare structural risk and workload features while accounting for compilation and resource costs.
