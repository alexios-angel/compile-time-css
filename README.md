> **Attribution:** this library is built on the CTLL compile-time LL(1)
> parser from [CTRE](https://github.com/hanickadot/compile-time-regular-expressions)
> by Hana Dusíková, via the [notre](https://github.com/alexios-angel/notre)
> fork, and follows the architecture of its siblings
> [compile-time-html](https://github.com/alexios-angel/compile-time-html),
> [compile-time-javascript](https://github.com/alexios-angel/compile-time-javascript) and
> [compile-time-json](https://github.com/alexios-angel/compile-time-json).
> Apache License 2.0 with LLVM Exceptions; see [NOTICE](NOTICE).

# ctcss — compile-time CSS

CSS parsed while your code compiles — and *resolved* there too. The
stylesheet is a *type*: malformed CSS is a compile error, every
accessor is `constexpr`, and selector matching plus the full cascade
(`!important`, specificity, source order) are plain `constexpr`
computations over element chains. Style a compile-time DOM in a
`static_assert`; run the very same `query()` at runtime when a script
has just flipped a class.

```c++
#include <ctcss.hpp>

constexpr auto theme = ctcss::parse<R"(
    p          { color: black; margin: 8px; }
    div.note p { color: red !important; }
    #main > ul { width: 640px; }
)">();

// the element chain body > div.note > p, root-first
constexpr ctcss::element_ref chain[] = {{"body"}, {"div", "", "note wide"}, {"p"}};

static_assert(ctcss::query(theme, chain, "color") == "red");    // cascade, resolved
static_assert(ctcss::query(theme, chain, "margin") == "8px");
static_assert(ctcss::parse_length("8px").value == 8);           // typed values
static_assert(ctcss::parse_color("#f80").g == 136);
static_assert(!ctcss::is_valid<"p { color red }">);             // typos fail the build
```

## What is supported (v0.1)

* **rules**: selector lists (`h1, .title { ... }`) with declaration
  blocks; the final semicolon is optional; `/* comments */` anywhere
* **selectors**: type (`div`), universal (`*`), `.class`, `#id`,
  compounds (`div.note#top.wide`), descendant combinator
  (whitespace), child combinator (`>`) — in any combination
* **declarations**: any `property: value` pair — property names fold
  to lowercase (they match case-insensitively), values keep their raw
  text exactly (font stacks, `url(...)`, shorthand lists all pass
  through), `!important` is peeled off and flagged
* **matching**: `ctcss::matches(selector, chain)` against
  `element_ref` chains (tag/id/space-separated classes — exactly what
  an HTML element knows); tags compare case-insensitively, classes
  and ids exactly; descendant matching backtracks correctly
* **the cascade**: `ctcss::query(sheet, chain, "property")` resolves
  `!important` → specificity (ids, classes, types) → source order,
  like a browser; `entries(sheet)` exposes the flattened
  (selector × declaration) view the cascade runs on
* **typed values**: `parse_length` (`px em rem % vw vh`, unitless
  zero) and `parse_color` (`#rgb #rgba #rrggbb #rrggbbaa`,
  `rgb()/rgba()`, the classic named colors, `transparent`) — all
  `constexpr`, ready for a layout engine
* **serialization**: `ctcss::serialize(sheet)` renders minified CSS
  into static storage

Not in v0.1 (all compile errors, documented): at-rules (`@media`,
`@import`, `@font-face`), pseudo-classes/elements (`:hover`,
`::before`), attribute selectors (`[href]`), sibling combinators
(`+`, `~`), CSS variables/`calc()` resolution, and semicolons/braces
inside quoted values.

## API

```c++
// syntax as a static property (never a compile error):
template <ctll::fixed_string Src> constexpr bool ctcss::is_valid;
ctcss::error_info<Src>();     // kind, byte offset, line, column
ctcss::error_message<Src>();  // rendered caret diagnostic

// the parsed sheet (a type); invalid CSS fails the build:
constexpr auto sheet = ctcss::parse<Src>();
sheet.rule_count();  sheet.rule_at<I>();
rule.selector_count();  rule.selector_at<I>();  rule.decl_count();  rule.decl<I>();
rule.property<"color">();          // raw value within this rule ("" if absent)
decl.property();  decl.value();  decl.important();
selector.specificity();            // ids*10000 + classes*100 + types
selector.step<I>();                // compound + how it attaches (descendant/child)

// matching and the cascade (equally valid in static_assert and at runtime):
ctcss::element_ref{tag, id, classes};              // classes space-separated
ctcss::matches(selector, chain);                   // chain root-first, self-last
ctcss::query(sheet, chain, "property");            // cascade-resolved value ("" if none)
ctcss::entries(sheet), ctcss::entry_count(sheet);  // the flattened cascade input

// typed values:
ctcss::parse_length("12px")   // -> {ok, value, unit}
ctcss::parse_color("#ff8800") // -> {ok, r, g, b, a}

// back to text:
ctcss::serialize(sheet);   // minified, static storage
```

`ctcss::for_each_rule(sheet, f)` iterates rules with their own types.
`ctcss::debug` bundles the [ctlark debugging
toolbox](../compile-time-lark#debugging) with the CSS grammar baked
in: `traced_parse<Src>()`, `parse_runtime(text)`,
`dump_tokens<Src>()`, `dump_grammar()`.

## C++17

With a pre-C++20 compiler, inputs and keys are `constexpr
ctll::fixed_string` variables with linkage instead of string literals;
matching and query are ordinary value calls and need nothing special:

```c++
static constexpr auto src = ctll::fixed_string{"p { color: red; }"};
constexpr auto sheet = ctcss::parse<src>();
constexpr ctcss::element_ref chain[] = {{"p"}};
static_assert(ctcss::query(sheet, chain, "color") == std::string_view{"red"});
```

## How it works

The grammar layer is
[ctlark](https://github.com/alexios-angel/compile-time-lark)
(compile-time Lark). Two tricks make CSS tractable
([`grammar.hpp`](include/ctcss/grammar.hpp)): a **compound selector is
a single token** — CSS's descendant combinator is *whitespace*, which
an `%ignore`-whitespace lexer would destroy, but with `div.note#top`
as one token, adjacency of compound tokens IS the descendant
combinator and `>` the child combinator — and a **declaration value is
a single raw token** up to the `;` or `}`, cooked later. The binder
([`bind.hpp`](include/ctcss/bind.hpp)) splits compounds into typed
tag/#id/.classes parts and peels `!important`.

Matching ([`match.hpp`](include/ctcss/match.hpp)) deliberately leaves
the type level: every selector and stylesheet materializes a flat
static VIEW (step tables; one `decl_entry` per selector ×
declaration), and `matches()`/`query()` are plain `constexpr` loops
over those views and an `element_ref` chain. That one design choice is
what lets the same code style a compile-time DOM inside a
`static_assert` and restyle a live one at runtime — the seam
[compile-time-browser](#roadmap) needs, since scripts mutate classes
at runtime.

Because the grammar work happens in headers, a **precompiled header**
makes it a one-time cost (`make pch`, automatic — the CSS grammar is
small, so this is quick), and an Earley parse needs a raised constexpr
budget, carried by the Makefile and the CMake interface
(`CTCSS_CONSTEXPR_LIMITS`):

```
clang:  -fconstexpr-steps=500000000 -fconstexpr-depth=1024 -fbracket-depth=2048
gcc:    -fconstexpr-ops-limit=3000000000 -fconstexpr-loop-limit=10000000 -fconstexpr-depth=1024
```

ctlark and ctll come in as a git submodule
(`external/compile-time-lark` — clone with `--recurse-submodules` or
run `git submodule update --init`); never edit under `external/`. The
build adds the submodule's include directories so the headers'
relative `"../ctlark.hpp"`-style includes resolve, and the CMake
install flattens everything back to `include/{ctcss,ctlark,ctll}`.

## Building and integrating

Header-only. Pick whichever fits your project:

**CMake, as a subdirectory or via FetchContent:**

```cmake
add_subdirectory(compile-time-css)   # or FetchContent_MakeAvailable(ctcss)
target_link_libraries(your-target PRIVATE ctcss::ctcss)
```

**CMake, installed** (`cmake -B build && cmake --install build`):

```cmake
find_package(ctcss 0.1 REQUIRED)
target_link_libraries(your-target PRIVATE ctcss::ctcss)
```

The install also ships a `pkg-config` file (`ctcss.pc`). Tests and
examples build only when ctcss is the top-level project
(`CTCSS_BUILD_TESTS`, `CTCSS_BUILD_EXAMPLES`); `CTCSS_CXX_STANDARD`
selects the advertised standard (default 20). CPack can produce
TGZ/ZIP archives (plus DEB/RPM where the tooling exists), and
`-DCTCSS_MODULE=ON` builds `ctcss.cppm` as a named C++ module
(experimental).

**No build system:** add `include/` plus the submodule's
`external/compile-time-lark/include` (and its `ctlark`/`ctll`
subdirectories) to your include path, or copy the amalgamated
[`single-header/ctcss.hpp`](single-header/ctcss.hpp)
(regenerate with `make single-header`, which needs the
[quom](https://pypi.org/project/quom/) tool).

Requires C++17 (C++20 for the string-literal API). Runnable demos live
in [`examples/`](examples/).

Run the tests (compilation is the test — the suite is `static_assert`s):

```bash
git submodule update --init            # ctlark + ctll (once, after cloning)
make CXX=clang++                       # C++20
make CXX=clang++ CXX_STANDARD=17
# or through CMake/CTest:
cmake -B build && cmake --build build && ctest --test-dir build
```

## Roadmap

ctcss is the third brick of a compile-time web stack, joining
[compile-time-html](https://github.com/alexios-angel/compile-time-html)
(the DOM as a type) and
[compile-time-javascript](https://github.com/alexios-angel/compile-time-javascript)
(scripts parsed at compile time, run at runtime). They meet in
**compile-time-browser**: HTML parsed into a DOM type, CSS resolved
against it at compile time, JS driving events at runtime — lowered
into an SDL3 application, as if the page had been hand-written as
native code. ctcss's `element_ref` chains are shaped exactly like
cthtml's elements (tag + id + class attribute), and its runtime-capable
`query()` is what restyles after a script mutation.

## License

Apache License 2.0 with LLVM Exceptions (see [LICENSE](LICENSE)).
The CTLL parser is Hana Dusíková's work, via notre; see
[NOTICE](NOTICE).
