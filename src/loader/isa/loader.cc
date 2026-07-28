#include "nigiri/loader/isa/loader.h"

#include "nigiri/loader/isa/load_timetable.h"

namespace nigiri::loader::isa {

bool isa_loader::applicable(dir const& d) const {
  return nigiri::loader::isa::applicable(d);
}

void isa_loader::load(loader_config const& c,
                      source_idx_t const src,
                      dir const& d,
                      timetable& tt,
                      hash_map<bitfield, bitfield_idx_t>& bitfield_indices,
                      assistance_times* assistance,
                      shapes_storage* shapes_data) const {
  return nigiri::loader::isa::load_timetable(c, src, d, tt, bitfield_indices,
                                             assistance, shapes_data);
}

std::string_view isa_loader::name() const { return "isa"; }

}  // namespace nigiri::loader::isa
