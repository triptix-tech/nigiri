#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "cista/containers/string.h"
#include "cista/reflection/for_each_field.h"

#include "date/date.h"

#include "geo/latlng.h"

#include "utl/parser/arg_parser.h"
#include "utl/parser/buf_reader.h"
#include "utl/parser/cstr.h"
#include "utl/parser/csv.h"
#include "utl/parser/line_range.h"
#include "utl/pipes/for_each.h"
#include "utl/pipes/is_range.h"
#include "utl/verify.h"

namespace nigiri::loader::isa {

template <typename T>
void parse_fields(utl::cstr&, T&);

template <typename G>
struct repeated {
  struct iterator {
    using value_type = G;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::input_iterator_tag;

    G operator*() const {
      auto s = rest_;
      auto g = G{};
      parse_fields(s, g);
      return g;
    }

    iterator& operator++() {
      auto g = G{};
      parse_fields(rest_, g);
      return *this;
    }

    void operator++(int) { ++*this; }

    friend bool operator==(iterator const& it, std::default_sentinel_t) {
      return it.rest_.len == 0U;
    }

    utl::cstr rest_{};
  };

  iterator begin() const { return {rest_}; }
  std::default_sentinel_t end() const { return {}; }

  std::size_t size() const {
    auto n = std::size_t{0U};
    for (auto it = begin(); it != end(); ++it) {
      ++n;
    }
    return n;
  }

  utl::cstr rest_{nullptr, 0U};
};

using timespan = std::chrono::seconds;
using time = ::date::hh_mm_ss<std::chrono::seconds>;
using date = ::date::year_month_day;

inline utl::cstr next_field(utl::cstr& s) {
  auto const token = get_until(s, '#');
  s += token.len;
  if (s.len != 0U) {
    ++s;
  }
  return token.trim();
}

constexpr auto const kEscapedSeparator = std::string_view{"\xC2\xA4"};

inline void unescape(utl::cstr const s, cista::raw::generic_string& v) {
  auto rest = std::string_view{s.str, s.len};
  auto pos = rest.find(kEscapedSeparator);
  if (pos == std::string_view::npos) {
    v.set_non_owning(s.str, static_cast<unsigned>(s.len));
    return;
  }
  auto out = std::string{};
  out.reserve(rest.size());
  while (pos != std::string_view::npos) {
    out += rest.substr(0U, pos);
    out += '#';
    rest = rest.substr(pos + kEscapedSeparator.size());
    pos = rest.find(kEscapedSeparator);
  }
  out += rest;
  v.set_owning(out);
}

inline void parse_value(utl::cstr& s, utl::cstr& v) { v = next_field(s); }

inline void parse_value(utl::cstr& s, cista::raw::generic_string& v) {
  auto const field = next_field(s);
  if (field.len != 0U) {
    unescape(field, v);
  }
}

template <typename T>
concept UtlParsable = requires(utl::cstr c, T& v) { utl::parse_value(c, v); };

template <UtlParsable T>
void parse_value(utl::cstr& s, T& v) {
  auto field = next_field(s);
  if (field.len != 0U) {
    utl::parse_value(field, v);
  }
}

inline void parse_value(utl::cstr& s, timespan& v) {
  auto const field = next_field(s);
  if (field.len == 0U) {
    return;
  }
  auto const m = get_until(field, ':');
  utl::verify(m.len != field.len, "invalid ZEITSPN '{}'", field.view());
  v = std::chrono::minutes{utl::parse<std::int32_t>(m)} +
      std::chrono::seconds{utl::parse<std::int32_t>(field.substr(m.len + 1U))};
}

inline void parse_value(utl::cstr& s, time& v) {
  auto const field = next_field(s);
  if (field.len == 0U) {
    return;
  }
  auto const h = get_until(field, '.');
  utl::verify(h.len != field.len, "invalid UHRZEIT '{}'", field.view());
  auto const rest = field.substr(h.len + 1U);
  auto const m = get_until(rest, ':');
  auto const sec =
      m.len == rest.len ? 0 : utl::parse<std::int32_t>(rest.substr(m.len + 1U));
  v = time{std::chrono::hours{utl::parse<std::int32_t>(h)} +
           std::chrono::minutes{utl::parse<std::int32_t>(m)} +
           std::chrono::seconds{sec}};
}

inline void parse_value(utl::cstr& s, date& v) {
  auto const field = next_field(s);
  if (field.len == 0U) {
    return;
  }
  auto const d = get_until(field, '.');
  utl::verify(d.len != field.len, "invalid DATUM '{}'", field.view());
  auto const rest = field.substr(d.len + 1U);
  auto const m = get_until(rest, '.');
  utl::verify(m.len != rest.len, "invalid DATUM '{}'", field.view());
  v = date{::date::year{utl::parse<std::int32_t>(rest.substr(m.len + 1U))},
           ::date::month{utl::parse<unsigned>(m)},
           ::date::day{utl::parse<unsigned>(d)}};
}

inline void parse_value(utl::cstr& s, geo::latlng& v) {
  auto x = next_field(s);
  auto y = next_field(s);
  if (x.len != 0U) {
    utl::parse_fp(x, v.lng_);
  }
  if (y.len != 0U) {
    utl::parse_fp(y, v.lat_);
  }
}

template <typename G>
void parse_value(utl::cstr& s, repeated<G>& v) {
  v.rest_ = s;
  s += s.len;
}

template <typename T>
void parse_fields(utl::cstr& s, T& t) {
  cista::for_each_field(t, [&](auto& f) {
    if constexpr (requires { (parse_value)(s, f); }) {
      (parse_value)(s, f);
    } else if constexpr (requires { (parse_value)(s, f.val()); }) {
      (parse_value)(s, f.val());
    }
  });
}

template <typename T>
T read_row(utl::cstr s) {
  auto t = T{};
  parse_fields(s, t);
  return t;
}

template <typename E>
struct counted_group_range {
  struct iterator {
    using value_type = E;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::input_iterator_tag;

    E operator*() const {
      auto line = utl::get_line(rest_);
      if (line && line[0] == '#') {
        line = line.substr(1);
      }
      return read_row<E>(line);
    }

    iterator& operator++() {
      skip_line();
      while (rest_.len != 0U && rest_[0] == '%') {
        skip_line();
      }
      return *this;
    }

    void operator++(int) { ++*this; }

    void skip_line() {
      if (rest_.len == 0U) {
        return;
      }
      auto const nl =
          static_cast<char const*>(std::memchr(rest_.str, '\n', rest_.len));
      rest_ = nl == nullptr
                  ? utl::cstr{rest_.str + rest_.len, std::size_t{0U}}
                  : utl::cstr{nl + 1, static_cast<std::size_t>(
                                          (rest_.str + rest_.len) - (nl + 1))};
    }

    friend bool operator==(iterator const& it, std::default_sentinel_t) {
      return it.rest_.len == 0U;
    }

    utl::cstr rest_{};
  };

  iterator begin() const { return {slice_}; }
  std::default_sentinel_t end() const { return {}; }

  utl::cstr slice_{nullptr, 0U};
};

template <typename T>
concept IsCountedGroup = requires(T t) {
  typename T::counted_group_element_t;
  { t.get_count() } -> std::convertible_to<std::size_t>;
  t.set_counted_group(
      counted_group_range<typename T::counted_group_element_t>{});
};

template <typename T>
concept IsMarkedGroup = requires(T t) {
  typename T::marked_group_element_t;
  t.set_marked_group(counted_group_range<typename T::marked_group_element_t>{});
};

template <typename T, typename LineRange>
struct isa_range : public LineRange {
  using result_t = T;

  isa_range(LineRange&& r) : LineRange{std::forward<LineRange>(r)} {}

  std::optional<utl::cstr> next_line() {
    if (pending_.has_value()) {
      auto const l = *pending_;
      pending_.reset();
      return l;
    }

    auto s = utl::cstr{};
    LineRange::next(s);
    s = utl::strip_cr(s);
    while (LineRange::valid(s) && s && s[0] == '%') {
      LineRange::next(s);
      s = utl::strip_cr(s);
    }

    if (LineRange::valid(s) && s) {
      return s;
    }

    return std::nullopt;
  }

  static utl::cstr slice(utl::cstr const first, utl::cstr const last) {
    return first.valid()
               ? utl::cstr{first.str, static_cast<std::size_t>(
                                          (last.str + last.len) - first.str)}
               : utl::cstr{nullptr, 0U};
  }

  std::optional<T> next_row() {
    auto const line = next_line();
    if (!line.has_value()) {
      return std::nullopt;
    }

    auto t = read_row<T>(*line);

    if constexpr (IsCountedGroup<T>) {
      auto first = utl::cstr{};
      auto last = utl::cstr{};
      auto const n = static_cast<std::size_t>(t.get_count());
      for (auto i = std::size_t{0U}; i != n; ++i) {
        auto const l = next_line();
        utl::verify(l.has_value(), "counted group ended after {}/{} rows", i,
                    n);
        if (i == 0U) {
          first = *l;
        }
        last = *l;
      }
      t.set_counted_group({slice(first, last)});
    } else if constexpr (IsMarkedGroup<T>) {
      auto first = utl::cstr{};
      auto last = utl::cstr{};
      while (true) {
        auto const l = next_line();
        if (!l.has_value()) {
          break;
        }
        if ((*l)[0] != '#') {
          pending_ = *l;
          break;
        }
        if (!first.valid()) {
          first = *l;
        }
        last = *l;
      }
      t.set_marked_group({slice(first, last)});
    }

    return t;
  }

  std::optional<T> begin() { return next_row(); }

  template <typename It>
  void next(It& it) {
    it = next_row();
  }

  template <typename It>
  auto&& read(It& it) const {
    return *it;
  }

  template <typename It>
  bool valid(It& it) const {
    return it.has_value();
  }

  std::optional<utl::cstr> pending_{};
};

template <typename T>
struct isa {
  template <typename LineRange>
  friend auto operator|(LineRange&& r, isa&&) {
    return isa_range<T, LineRange>{std::forward<LineRange>(r)};
  }
};

template <typename T, typename Fn>
void for_each_row(std::string_view file_content, Fn&& fn) {
  utl::line_range{utl::make_buf_reader(file_content)}  //
      | isa<T>()  //
      | utl::for_each(std::forward<Fn>(fn));
}

}  // namespace nigiri::loader::isa

namespace utl {

template <typename T, typename LineRange>
struct is_range<nigiri::loader::isa::isa_range<T, LineRange>> : std::true_type {
};

}  // namespace utl
