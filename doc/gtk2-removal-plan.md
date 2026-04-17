# GTK2 Removal Plan

## Goal
Remove all GTK2 support and GTK2-specific UI files, keeping talculator GTK3-only.

## Scope
- Build/configure logic (`configure.ac`)
- UI selection macros (`src/talculator.h`)
- Installed UI file list (`ui/Makefile.am`)
- GTK2 UI assets under `ui/`
- Remaining `GTK_CHECK_VERSION` and GTK2-only runtime branches in `src/`

## Phase 1: GTK3-only baseline
1. Remove `--enable-gtk3` switch from `configure.ac`.
2. Require `gtk+-3.0` unconditionally via `PKG_CHECK_MODULES`.
3. Update comments in `configure.ac` to reflect GTK3-only support.
4. Update `src/talculator.h` to always use GTK3 UI files (`*_gtk3.ui`, `prefs_gtk3.ui`) outside `WITH_HILDON`.

## Phase 2: Remove GTK2 UI files
1. Delete GTK2 UI files:
   - `ui/basic_buttons_gtk2.ui`
   - `ui/scientific_buttons_gtk2.ui`
   - `ui/dispctrl_bottom_gtk2.ui`
   - `ui/dispctrl_right_gtk2.ui`
   - `ui/dispctrl_right_vertical_gtk2.ui`
   - `ui/prefs_gtk2.ui`
2. Remove those files from `ui/Makefile.am`.
3. Keep GTK3 files and hildon files intact.

## Phase 3: Remove GTK2 runtime branches
1. Remove GTK2-only key snooper path in `src/main.c`.
2. Remove `#if GTK_CHECK_VERSION(3, 0, 0)` split branches that now only gate GTK3 logic.
3. Collapse menu popup and style code to GTK3 path only.
4. Keep behavior parity for key handling and display updates.

## Phase 4: Build-system regeneration
1. Run `autoreconf -fi`.
2. Re-run `./configure --prefix=/usr`.
3. Build and run tests.
4. Update generated autotools files tracked in repo (`configure`, `Makefile.in`, etc.) as needed.

## Phase 5: Validation
1. Smoke test all modes: basic, scientific, paper.
2. Verify ESC/input behavior and edit menu behavior.
3. Verify tabs and per-tab state.
4. Verify install/package path still includes required GTK3 UI assets.

## Risks
- GTK2 branches may currently carry behavior quirks that accidentally mask bugs.
- Removing stale UI files without regenerating autotools files can cause packaging/install mismatch.
- Hildon path should remain untouched unless explicitly dropped later.

