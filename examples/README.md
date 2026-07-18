# Examples

Self-contained programs, each compilable against `../include` (or the
single header). Build with `make`, build and run with `make run`; they
also build and run as tests through CMake/CTest.

| File | Shows |
|------|-------|
| [`theme.cpp`](theme.cpp) | the browser story in miniature: a theme baked in at compile time, style resolution (matching, specificity, `!important`) proven in `static_assert`s, and the SAME `query()` restyling at runtime after a dark-mode class flip |
| [`wellformed.cpp`](wellformed.cpp) | `is_valid` as a bool: what parses, the one-typo-away failures, and the compile-time caret diagnostics |
