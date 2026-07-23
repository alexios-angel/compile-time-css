#ifndef CTCSS__HPP
#define CTCSS__HPP

#include <cstddef>

#include "ctcss/types.hpp"
#include "ctcss/values.hpp"
#include "ctcss/match.hpp"
#include "ctcss/value.hpp"

// ctcss: compile-time CSS.
//
//   constexpr std::string_view css = R"(
//       p          { color: black; margin: 8px; }
//       div.note p { color: red !important; }
//       #main      { width: 640px; }
//   )";
//   constexpr ctcss::element_ref chain[] = {
//       {"div", "", "note"}, {"p", "", ""}};
//   static_assert(ctcss::query(ctcss::parse_value(css), chain, "color") == "red");
//
// One parser, usable both ways: parse_value(std::string_view) is a
// constexpr VALUE parser - resolve the cascade in a static_assert, or
// load a runtime stylesheet with the same call. Selector matching,
// specificity and source-order tie-breaks run as ordinary value loops
// (query); parse_length/parse_color cook the winning declaration's
// text on demand. At-rules are handled leniently: applying @media
// blocks flatten in, @keyframes and @font-face are captured for the
// animation engine.

#endif
