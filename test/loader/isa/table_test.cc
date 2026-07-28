#include <ranges>

#include "gtest/gtest.h"

#include "nigiri/loader/isa/table.h"

#include "test_strings.h"

using namespace nigiri::loader::isa;

namespace {

struct lieferant {
  utl::cstr code_;
  utl::cstr name_;
  unsigned address_{};
};

struct ort {
  utl::cstr id_;
  utl::cstr name_;
  utl::cstr gkz_;
};

struct notice {
  cista::raw::generic_string text_;
};

struct named_lieferant {
  utl::cstr code_;
  cista::raw::generic_string name_;
  unsigned address_{};
};

bool points_into(char const* const p, std::string_view const s) {
  return p >= s.data() && p < s.data() + s.size();
}

struct ld_stop_row {
  unsigned seq_{};
  utl::cstr code_;
  unsigned stop_{};
  unsigned km_{};
  utl::cstr arr_pos_;
  utl::cstr dep_pos_;
  utl::cstr travel_;
};

struct fd_trip {
  unsigned from_pos_{};
  unsigned from_stop_{};
  utl::cstr dep_;
  unsigned to_pos_{};
  unsigned to_stop_{};
  utl::cstr arr_;
  utl::cstr vm_;
  unsigned profile_{};
  utl::cstr external_;
  utl::cstr day_types_;
  unsigned n_following_{};
  utl::cstr headway_;
  unsigned bitfield_{};
  utl::cstr internal_;
  utl::cstr type_;
};

struct fd_header {
  using counted_group_element_t = fd_trip;

  std::size_t get_count() const { return n_trips_; }
  void set_counted_group(counted_group_range<fd_trip> const g) { trips_ = g; }

  utl::cstr line_;
  unsigned version_{};
  utl::cstr betriebsteil_;
  utl::cstr direction_;
  unsigned subline_{};
  unsigned n_trips_{};
  counted_group_range<fd_trip> trips_;
};

struct ld_profile {
  utl::cstr travel_;
  utl::cstr wait_;
  utl::cstr no_entry_;
  utl::cstr no_exit_;
  utl::cstr demand_;
};

struct ld_data_row {
  unsigned seq_{};
  utl::cstr code_;
  unsigned stop_{};
  utl::cstr km_;
  utl::cstr arr_pos_;
  utl::cstr dep_pos_;
  repeated<ld_profile> profiles_;
};

struct ld_header {
  using counted_group_element_t = ld_data_row;

  std::size_t get_count() const { return n_stops_; }
  void set_counted_group(counted_group_range<ld_data_row> const g) {
    stops_ = g;
  }

  utl::cstr line_;
  unsigned version_{};
  utl::cstr betriebsteil_;
  unsigned subline_{};
  utl::cstr direction_;
  unsigned n_stops_{};
  unsigned n_profiles_{};
  utl::cstr vm_;
  counted_group_range<ld_data_row> stops_;
};

struct linie_version {
  unsigned prio_{};
  unsigned version_{};
  unsigned bitfield_{};
};

struct linie {
  using marked_group_element_t = linie_version;

  void set_marked_group(counted_group_range<linie_version> const g) {
    versions_ = g;
  }

  utl::cstr betriebsteil_;
  utl::cstr number_;
  utl::cstr name_;
  utl::cstr type_;
  utl::cstr vm_gruppe_;
  utl::cstr dlid_;
  counted_group_range<linie_version> versions_;
};

static_assert(std::ranges::input_range<counted_group_range<fd_trip>>);
static_assert(std::ranges::input_range<repeated<ld_profile>>);

template <typename R>
std::vector<std::ranges::range_value_t<R>> collect(R const& r) {
  auto v = std::vector<std::ranges::range_value_t<R>>{};
  for (auto&& e : r) {
    v.emplace_back(e);
  }
  return v;
}

}  // namespace

TEST(isa_table, flat) {
  auto rows = std::vector<lieferant>{};
  for_each_row<lieferant>(kFlat,
                          [&](lieferant const& l) { rows.emplace_back(l); });
  ASSERT_EQ(1U, rows.size());
  EXPECT_EQ("NASA", rows[0].code_.view());
  EXPECT_EQ("NASA", rows[0].name_.view());
  EXPECT_EQ(227U, rows[0].address_);
}

TEST(isa_table, comment_crlf_eof) {
  auto rows = std::vector<ort>{};
  for_each_row<ort>(kCommentCrlfEof,
                    [&](ort const& o) { rows.emplace_back(o); });
  ASSERT_EQ(1U, rows.size());
  EXPECT_EQ("1", rows[0].id_.view());
  EXPECT_EQ("Döllnitz", rows[0].name_.view());
  EXPECT_EQ("15088330", rows[0].gkz_.view());
}

TEST(isa_table, empty_fields) {
  auto rows = std::vector<ld_stop_row>{};
  for_each_row<ld_stop_row>(
      kEmptyFields, [&](ld_stop_row const& r) { rows.emplace_back(r); });
  ASSERT_EQ(1U, rows.size());
  EXPECT_EQ(1U, rows[0].seq_);
  EXPECT_EQ(0U, rows[0].code_.len);
  EXPECT_EQ(8011988U, rows[0].stop_);
  EXPECT_EQ(0U, rows[0].km_);
  EXPECT_EQ(0U, rows[0].arr_pos_.len);
  EXPECT_EQ(0U, rows[0].dep_pos_.len);
  EXPECT_EQ("06:00", rows[0].travel_.view());
}

TEST(isa_table, escape_allocates) {
  auto rows = std::vector<notice>{};
  for_each_row<notice>(kEscape, [&](notice const& n) { rows.emplace_back(n); });
  ASSERT_EQ(1U, rows.size());
  EXPECT_EQ("Fahrplan gilt # Hinweis beachten", rows[0].text_.view());
  EXPECT_FALSE(points_into(rows[0].text_.data(), kEscape));
}

TEST(isa_table, no_escape_zero_copy) {
  constexpr auto const kLongName =
      "MASTER    #NASA Master-Lieferant Landesnahverkehrsgesellschaft mbH##\n";
  auto rows = std::vector<named_lieferant>{};
  for_each_row<named_lieferant>(
      kLongName, [&](named_lieferant const& l) { rows.emplace_back(l); });
  ASSERT_EQ(1U, rows.size());
  EXPECT_EQ("NASA Master-Lieferant Landesnahverkehrsgesellschaft mbH",
            rows[0].name_.view());
  EXPECT_TRUE(points_into(rows[0].name_.data(), kLongName));

  auto short_rows = std::vector<named_lieferant>{};
  for_each_row<named_lieferant>(
      kFlat, [&](named_lieferant const& l) { short_rows.emplace_back(l); });
  ASSERT_EQ(1U, short_rows.size());
  EXPECT_EQ("NASA", short_rows[0].name_.view());
  EXPECT_EQ(227U, short_rows[0].address_);
}

TEST(isa_table, counted_group) {
  auto groups = std::vector<fd_header>{};
  for_each_row<fd_header>(kFdCountedGroup,
                          [&](fd_header const& h) { groups.emplace_back(h); });

  ASSERT_EQ(2U, groups.size());

  EXPECT_EQ("S8", groups[0].line_.view());
  EXPECT_EQ(6U, groups[0].subline_);
  auto const trips_a = collect(groups[0].trips_);
  ASSERT_EQ(2U, trips_a.size());
  EXPECT_EQ("23.32:00", trips_a[0].dep_.view());
  EXPECT_EQ(8010085U, trips_a[0].to_stop_);
  EXPECT_EQ(14604U, trips_a[0].bitfield_);
  EXPECT_EQ("LF", trips_a[0].type_.view());
  EXPECT_EQ("22.32:00", trips_a[1].dep_.view());
  EXPECT_EQ(1159U, trips_a[1].bitfield_);

  EXPECT_EQ("RE1", groups[1].line_.view());
  auto const trips_b = collect(groups[1].trips_);
  ASSERT_EQ(1U, trips_b.size());
  EXPECT_EQ("10.00:00", trips_b[0].dep_.view());
  EXPECT_EQ(63U, trips_b[0].bitfield_);
}

TEST(isa_table, counted_group_with_repeated_fields) {
  auto groups = std::vector<ld_header>{};
  for_each_row<ld_header>(kLdRepeatedGroup,
                          [&](ld_header const& h) { groups.emplace_back(h); });

  ASSERT_EQ(1U, groups.size());
  EXPECT_EQ("S8", groups[0].line_.view());
  EXPECT_EQ(2U, groups[0].n_stops_);
  EXPECT_EQ(2U, groups[0].n_profiles_);

  auto const stops = collect(groups[0].stops_);
  ASSERT_EQ(2U, stops.size());

  EXPECT_EQ(8011988U, stops[0].stop_);
  auto const p0 = collect(stops[0].profiles_);
  ASSERT_EQ(2U, p0.size());
  EXPECT_EQ("05:00", p0[0].travel_.view());
  EXPECT_EQ("06:00", p0[1].travel_.view());

  EXPECT_EQ(8011199U, stops[1].stop_);
  auto const p1 = collect(stops[1].profiles_);
  ASSERT_EQ(2U, p1.size());
  EXPECT_EQ(0U, p1[0].travel_.len);
  EXPECT_EQ("00:00", p1[0].wait_.view());
  EXPECT_EQ("1", p1[0].demand_.view());
  EXPECT_EQ("00:30", p1[1].wait_.view());
}

TEST(isa_table, marked_group) {
  auto groups = std::vector<linie>{};
  for_each_row<linie>(kLinienHeadedGroup,
                      [&](linie const& l) { groups.emplace_back(l); });

  ASSERT_EQ(2U, groups.size());

  EXPECT_EQ("HVG___", groups[0].betriebsteil_.view());
  EXPECT_EQ("14", groups[0].number_.view());
  auto const versions_a = collect(groups[0].versions_);
  ASSERT_EQ(2U, versions_a.size());
  EXPECT_EQ(1U, versions_a[0].prio_);
  EXPECT_EQ(1U, versions_a[0].version_);
  EXPECT_EQ(0U, versions_a[0].bitfield_);
  EXPECT_EQ(2U, versions_a[1].prio_);
  EXPECT_EQ(2U, versions_a[1].version_);
  EXPECT_EQ(42U, versions_a[1].bitfield_);

  EXPECT_EQ("BLK___", groups[1].betriebsteil_.view());
  auto const versions_b = collect(groups[1].versions_);
  ASSERT_EQ(1U, versions_b.size());
}
