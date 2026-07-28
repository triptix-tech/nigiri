#include "nigiri/loader/isa/load_timetable.h"

#include "fmt/format.h"

#include "utl/verify.h"

#include "nigiri/loader/gtfs/tz_map.h"
#include "nigiri/loader/isa/table.h"
#include "nigiri/loader/register.h"
#include "nigiri/logging.h"
#include "nigiri/timetable.h"

namespace nigiri::loader::isa {

bool applicable(dir const& d) {
  return d.exists("zeichen.asc") && d.exists("halteste.asc") &&
         d.exists("bitfeld.asc");
}

namespace {

struct stop_key {
  std::string_view org_;
  std::uint32_t id_{};
};

using stop_map_t = hash_map<stop_key, location_idx_t>;

struct zeichen_row {
  utl::cstr zeichensatz_;
  utl::cstr versionsnummer_;
  utl::cstr inkrementell_;
  utl::cstr zeitzone_;
};

struct halteste_row {
  std::uint32_t haltestellennummer_{};
  utl::cstr lieferantenkuerzel_;
  std::optional<std::uint32_t> referenzhaltestellennummer_;
  utl::cstr lieferantenkuerzel_referenzhaltestelle_;
  utl::cstr haltestellentyp_;
  utl::cstr haltestellenkuerzel_;
  geo::latlng koordinate_;
  utl::cstr gemeindekennziffer_;
  utl::cstr behindertengerecht_;
  cista::raw::generic_string haltestellenlangname_;
  utl::cstr zielbeschilderung_;
  utl::cstr auskunftsname_;
  utl::cstr satzname_;
  utl::cstr kminfo_wert_;
  utl::cstr bfprio_wert_;
  utl::cstr exportflag_;
  utl::cstr rbl_nummer_;
  utl::cstr ortstyp_;
  utl::cstr globale_id_;
  utl::cstr auswahlbeschraenkung_;
  utl::cstr anroutbeschraenkung_;
  utl::cstr iv_routing_;
  std::optional<timespan> umsteigezeit_;
  utl::cstr umsteigezeit_ic_;
  utl::cstr zhv_meldeflag_;
  utl::cstr zeitzone_;
};

zeichen_row parse_zeichen(std::string_view const zeichen_content) {
  auto z = zeichen_row{};
  for_each_row<zeichen_row>(zeichen_content,
                            [&](zeichen_row const& r) { z = r; });
  return z;
}

stop_map_t parse_stops(loader_config const& c,
                       source_idx_t const src,
                       timetable& tt,
                       zeichen_row const& zeichen,
                       std::string_view const halteste_content) {
  auto tz_names = gtfs::tz_map{};
  auto const default_tz_name = zeichen.zeitzone_.len != 0U
                                   ? zeichen.zeitzone_.view()
                                   : std::string_view{c.default_tz_};
  utl::verify(!default_tz_name.empty(),
              "isa: no timezone in zeichen.asc and no default timezone set");
  auto const default_tz = gtfs::get_tz_idx(tt, tz_names, default_tz_name);

  auto map = stop_map_t{};
  auto parent_refs = std::vector<std::pair<location_idx_t, stop_key>>{};
  for_each_row<halteste_row>(halteste_content, [&](halteste_row const& r) {
    auto const id = fmt::format("{}:{}", r.lieferantenkuerzel_.view(),
                                r.haltestellennummer_);
    auto const l = register_location(
        tt,
        {tt, src, id, tt.register_translation(r.haltestellenlangname_.view()),
         kEmptyTranslation,
         r.haltestellenkuerzel_.len != 0U
             ? tt.register_translation(r.haltestellenkuerzel_.view())
             : kEmptyTranslation,
         kEmptyTranslation, r.koordinate_,
         r.referenzhaltestellennummer_.has_value() ? location_type::kTrack
                                                   : location_type::kStation,
         location_idx_t::invalid(),
         r.zeitzone_.len != 0U
             ? gtfs::get_tz_idx(tt, tz_names, r.zeitzone_.view())
             : default_tz,
         std::chrono::duration_cast<duration_t>(
             r.umsteigezeit_.value_or(c.default_transfer_time_)),
         tz_names});
    map.emplace(stop_key{r.lieferantenkuerzel_.view(), r.haltestellennummer_},
                l);
    if (r.referenzhaltestellennummer_.has_value()) {
      auto const parent_org =
          r.lieferantenkuerzel_referenzhaltestelle_.len != 0U
              ? r.lieferantenkuerzel_referenzhaltestelle_
              : r.lieferantenkuerzel_;
      parent_refs.emplace_back(
          l, stop_key{parent_org.view(), *r.referenzhaltestellennummer_});
    }
  });

  auto n_unresolved_parents = 0U;
  for (auto const& [child, parent_key] : parent_refs) {
    auto const it = map.find(parent_key);
    if (it == end(map)) {
      ++n_unresolved_parents;
      continue;
    }
    tt.locations_.parents_[child] = it->second;
    tt.locations_.children_[it->second].emplace_back(child);
  }

  if (n_unresolved_parents != 0U) {
    log(log_lvl::error, "loader.isa.stops", "{} unresolved parent references",
        n_unresolved_parents);
  }

  return map;
}

}  // namespace

void load_timetable(loader_config const& c,
                    source_idx_t const src,
                    dir const& d,
                    timetable& tt,
                    hash_map<bitfield, bitfield_idx_t>&,
                    assistance_times*,
                    shapes_storage*) {
  auto const zeichen_file = d.get_file("zeichen.asc");
  auto const zeichen = parse_zeichen(zeichen_file.data());

  auto const halteste = d.get_file("halteste.asc");
  [[maybe_unused]] auto const stops =
      parse_stops(c, src, tt, zeichen, halteste.data());

  throw utl::fail("ISA loader: not implemented");
}

}  // namespace nigiri::loader::isa
