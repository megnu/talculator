# libqalculate Port Plan (UI-Preserving)

## Goal
Replace talculator's legacy mixed `G_REAL` evaluation core with a libqalculate-backed engine while preserving the existing GTK UI and tabbed UX.

## Why this is non-trivial
Today these concerns are tightly coupled around `G_REAL`:
- arithmetic and scientific ops
- bitwise/base conversion logic
- parser/tokenization
- RPN stack behavior
- display formatting and precision decisions

This is spread across:
- `src/g_real.h`
- `src/calc_basic.c`
- `src/flex_parser.l`
- `src/general_functions.c`
- `src/display.c`
- `src/callbacks.c`

libqalculate already solves these domains, but with different semantics and a C++ API.

## Strategy
1. Add an engine abstraction seam (`src/engine.h/.c`) between UI and math core.
2. Keep legacy backend available during migration.
3. Add a libqalculate backend implementation behind the same interface.
4. Migrate feature-by-feature and compare outputs.

## Proposed phases

### Phase 0: Scaffolding
- Introduce engine API and backend selection.
- No behavior change.

### Phase 1: Decimal expression evaluation
- Route formula/basic decimal expression evaluation through libqalculate.
- Keep legacy for bitwise/base/RPN initially.

### Phase 2: Formatting and display parity
- Map talculator display prefs to libqalculate `PrintOptions`.
- Keep existing UI layout/events unchanged.

### Phase 3: Base + bitwise semantics
- Migrate `HEX/OCT/BIN`, shifts, and bitwise operations.
- Define compatibility policy for non-integer and overflow cases.

### Phase 4: RPN + memory parity
- Map RPN stack operations to libqalculate RPN APIs (or emulate via MathStructure stack).
- Preserve current tab-local memory and per-tab runtime state.

### Phase 5: Cleanup
- Remove obsolete `G_REAL`-dependent evaluation paths once parity is accepted.

## Behavior compatibility decisions to make early
- Percent semantics (`x%y` vs calculator-style context-sensitive `%` key).
- Exact vs approximate default output.
- Error message style and when to show parser/eval warnings.
- Base conversion edge cases and signed-width behavior.

## Risks
- C (UI) to C++ (libqalculate) integration boundary.
- Subtle UX regressions from semantic differences.
- Significant surface area in callbacks/display update flow.

## Success criteria
- Existing UI remains intact.
- Deterministic behavior across tabs/modes.
- Regression suite passes for:
  - arithmetic
  - trig/log/exponential
  - base/bitwise
  - constants and user functions
  - RPN operations
  - formatting and error paths
