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

Phase 3 status (completed on `libqalculate` branch):
- PAN/formula button operators for `<<`, `>>`, `AND`, `OR`, `XOR`, and `MOD` now route through the engine/libqalculate expression path.
- Base-aware parse/print settings are now mapped into libqalculate (`ParseOptions` and `PrintOptions`) using current talculator prefs.

Phase 3 compatibility policy:
- Active base drives parsing (`DEC/HEX/OCT/BIN`) for engine-evaluated expressions.
- `BIN` and `HEX` signed + bit-width behavior follows prefs by applying two's-complement and configured bit width in libqalculate parse/print options.
- `OCT` keeps base-8 parsing/printing; signed-width behavior continues to follow current legacy display/conversion paths where applicable.
- For formula mode, free-typed expressions follow libqalculate parser semantics; button-inserted logical/shift operators are normalized to libqalculate-compatible tokens.

### Phase 4: RPN + memory parity
- Map RPN stack operations to libqalculate RPN APIs (or emulate via MathStructure stack).
- Preserve current tab-local memory and per-tab runtime state.

Phase 4 status (completed on `libqalculate` branch):
- RPN runtime state is now persisted per tab context and restored on tab activation (instead of sharing one global transient stack across tabs).
- Tab switching/binding paths now snapshot active-tab RPN stack and rehydrate target-tab RPN stack when the target notation is RPN.
- Tab close flow now safely snapshots/remaps RPN runtime when removing the active page.
- Memory behavior remains tab-local via `tab_memory` and was preserved unchanged.

### Phase 5: Cleanup
- Remove obsolete `G_REAL`-dependent evaluation paths once parity is accepted.

Phase 5 status (completed on `libqalculate` branch):
- Removed legacy expression-evaluator backend execution path from `engine.c` (no more `flex_parser` execution via engine abstraction).
- Made libqalculate the required engine backend at startup; legacy fallback path removed.
- Backend availability failure is now a hard startup error instead of a silent fallback to legacy behavior.

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

## Deferred cleanup (post-port)
- GTK deprecation migration is intentionally deferred:
  - `gtk_menu_popup*` -> `gtk_menu_popup_at_pointer` / `gtk_menu_popup_at_widget`
  - `gtk_font_button_get/set_font_name` -> `gtk_font_chooser_get/set_font`
  - `gtk_widget_override_*` color/font APIs -> CSS provider/style-class based styling
  - `gdk_screen_width` -> monitor geometry APIs
- These are non-blocking for libqalculate math-core parity and will be handled in a dedicated UI modernization pass.

## Deferred legacy compatibility notes
- Keep current old-config migration behavior for now:
  - fallback read from legacy path (`CONFIG_FILE_NAME_OLD`) in `src/main.c`
  - compatibility parsing of old preference keys in `src/config_file.c` (`prefs_list_old_entries`)
- Keep GTK2/older compatibility preprocessor branches for now.
- These are not part of the math-core port and can be removed in a separate modernization pass once backward-compatibility support is no longer required.
