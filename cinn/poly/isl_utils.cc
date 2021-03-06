#include "cinn/poly/isl_utils.h"

#include <glog/logging.h>
#include <isl/cpp.h>

#include <algorithm>

#include "cinn/utils/string.h"

namespace cinn {
namespace poly {
using utils::Join;
using utils::StringFormat;

std::vector<std::string> isl_get_dim_names(const isl::set &x) {
  std::vector<std::string> res;
  for (int i = 0; i < isl_set_dim(x.get(), isl_dim_set); i++) {
    res.push_back(isl_set_get_dim_name(x.get(), isl_dim_set, i));
  }
  return res;
}

std::vector<std::string> isl_get_dim_names(const isl::map &x, isl_dim_type dim_type) {
  std::vector<std::string> res;
  for (int i = 0; i < isl_map_dim(x.get(), dim_type); i++) {
    res.push_back(isl_map_get_dim_name(x.get(), dim_type, i));
  }
  return res;
}

std::vector<std::string> isl_get_dim_names(isl_set *set) {
  std::vector<std::string> res;
  for (int i = 0; i < isl_set_dim(set, isl_dim_set); i++) {
    res.push_back(isl_set_get_dim_name(set, isl_dim_set, i));
  }
  return res;
}

void isl_set_dim_names(isl::map *map, isl_dim_type dim_type, const std::vector<std::string> &names) {
  const int dim = isl_map_dim(map->get(), dim_type);
  CHECK_EQ(dim, names.size());

  for (int i = 0; i < dim; i++) {
    *map = isl::manage(isl_map_set_dim_name(map->release(), dim_type, i, names[i].c_str()));
  }
}

void isl_set_dim_names(isl::set *set, const std::vector<std::string> &names) {
  int dim = isl_set_dim(set->get(), isl_dim_set);
  CHECK_EQ(dim, names.size());

  for (int i = 0; i < dim; i++) {
    *set = isl::manage(isl_set_set_dim_name(set->release(), isl_dim_set, i, names[i].c_str()));
  }
}

isl::union_map isl_maps_to_union_map(const std::vector<isl::map> &maps) {
  CHECK(!maps.empty());
  isl::union_map umap = isl::manage(isl_union_map_from_map(maps.front().copy()));
  for (int i = 1; i < maps.size(); i++) {
    umap = isl::manage(isl_union_map_add_map(umap.release(), maps[i].copy()));
  }
  return umap;
}

isl::union_set isl_sets_to_union_set(const std::vector<isl::set> &sets) {
  CHECK(!sets.empty());
  isl::union_set uset = isl::manage(isl_union_set_from_set(sets.front().copy()));
  for (int i = 1; i < sets.size(); i++) {
    uset = isl::manage(isl_union_set_add_set(uset.release(), sets[i].copy()));
  }
  return uset;
}

std::string isl_map_get_statement_repr(__isl_keep isl_map *map, isl_dim_type type) {
  CHECK(map);
  auto tuple_name = isl_map_get_tuple_name(map, type);
  std::vector<std::string> dims;

  for (int i = 0; i < isl_map_dim(map, type); i++) {
    dims.push_back(isl_map_get_dim_name(map, type, i));
  }
  return StringFormat("%s[%s]", tuple_name, Join(dims, ", ").c_str());
}

std::vector<std::string> isl_get_dim_names(isl_map *map, isl_dim_type dim_type) {
  std::vector<std::string> res;
  int n = isl_map_dim(map, dim_type);
  for (int i = 0; i < n; i++) {
    res.push_back(isl_map_get_dim_name(map, dim_type, i));
  }
  return res;
}

isl::set SetGetDims(isl::set set, const std::vector<int> &dims) {
  std::string tuple_name = isl_set_get_tuple_name(set.get());
  auto dim_names         = isl_get_dim_names(set);
  std::vector<std::string> selected_dim_names;
  for (int v : dims) {
    CHECK_LT(v, dim_names.size());
    selected_dim_names.push_back(dim_names[v]);
  }

  std::string transform_repr = StringFormat("{ %s[%s] -> %s[%s] }",
                                            tuple_name.c_str(),             //
                                            Join(dim_names, ", ").c_str(),  //
                                            tuple_name.c_str(),             //
                                            Join(selected_dim_names, ", ").c_str());
  isl::map transform(set.ctx(), transform_repr);
  return set.apply(transform);
}

isl_set *isl_get_precending_aixs(isl_set *set, int level, bool with_tuple_name) {
  int n = isl_set_dim(set, isl_dim_set);
  CHECK_LT(level, n);

  std::vector<std::string> domain_iterators;
  std::vector<std::string> range_iterators;

  for (int i = 0; i < n; i++) {
    domain_iterators.push_back("i" + std::to_string(i));
  }

  for (int i = 0; i < level; i++) {
    range_iterators.push_back("i" + std::to_string(i));
  }

  const char *statement = isl_set_get_tuple_name(set);

  std::string repr = utils::StringFormat("{ %s[%s] -> %s[%s] }",
                                         statement,
                                         utils::Join(domain_iterators, ", ").c_str(),
                                         statement,
                                         utils::Join(range_iterators, ", ").c_str());
  auto transform   = isl::manage(isl_map_read_from_str(isl_set_get_ctx(set), repr.c_str()));

  return isl_set_apply(set, transform.release());
}

int isl_max_level_compatible(isl_set *a, isl_set *b) {
  int an = isl_set_dim(a, isl_dim_set);
  int bn = isl_set_dim(b, isl_dim_set);
  CHECK_GE(an, 0);
  CHECK_GE(bn, 0);

  int compatible_level = -1;
  for (int i = 0; i < std::min(an, bn); i++) {
    isl::set a_prefix = isl::manage(isl_get_precending_aixs(isl_set_copy(a), i, false));
    isl::set b_prefix = isl::manage(isl_get_precending_aixs(isl_set_copy(b), i, false));

    a_prefix = isl::manage(isl_set_set_tuple_name(a_prefix.release(), "s"));
    b_prefix = isl::manage(isl_set_set_tuple_name(b_prefix.release(), "s"));
    if (isl_set_is_equal(a_prefix.get(), b_prefix.get()))
      compatible_level = i;
    else
      break;
  }

  return compatible_level;
}

isl_set *isl_remove_axis_by_name(isl_set *set, const char *axis_name) {
  std::string tuple_name = isl_set_get_tuple_name(set);
  int offset             = isl_set_find_dim_by_name(set, isl_dim_set, axis_name);
  set                    = isl_set_remove_dims(set, isl_dim_set, offset, 1);
  set                    = isl_set_set_tuple_name(set, tuple_name.c_str());
  return set;
}

isl_map *isl_remove_axis_by_name(isl_map *map, isl_dim_type dim_type, const char *axis_name) {
  int offset             = isl_map_find_dim_by_name(map, dim_type, axis_name);
  std::string tuple_name = isl_map_get_tuple_name(map, dim_type);
  map                    = isl_map_remove_dims(map, dim_type, offset, 1);
  map                    = isl_map_set_tuple_name(map, dim_type, tuple_name.c_str());
  return map;
}
isl_set *isl_rename_axis(isl_set *set, int offset, const char *name) {
  return isl_set_set_dim_name(set, isl_dim_set, offset, name);
}
isl_map *isl_rename_axis(isl_map *map, isl_dim_type dim_type, int offset, const char *name) {
  return isl_map_set_dim_name(map, dim_type, offset, name);
}

isl_set *isl_simplify(isl_set *set) {
  set = isl_set_coalesce(set);
  set = isl_set_remove_redundancies(set);
  return set;
}

std::tuple<isl::val, isl::val> isl_set_get_axis_range(isl_set *set, int pos) {
  CHECK(isl_set_dim_is_bounded(set, isl_dim_set, pos)) << "an unbound cannot get range, " << isl_set_to_str(set);

  std::vector<std::string> from_iters;
  std::string target_axis_name;
  for (int i = 0; i < isl_set_dim(set, isl_dim_set); i++) {
    auto *name = isl_set_get_dim_name(set, isl_dim_set, i);
    if (name) {
      from_iters.push_back(name);
    } else {
      from_iters.push_back("__emp__" + std::to_string(i));
    }
    if (pos == i) target_axis_name = from_iters.back();
  }

  isl::aff aff(isl_set_get_ctx(set),
               utils::StringFormat("{ %s[%s] -> [%s] }",
                                   isl_set_get_tuple_name(set),           // tuple
                                   utils::Join(from_iters, ",").c_str(),  // [...]
                                   target_axis_name.c_str()));

  isl::val max_val = isl::manage(isl_set_max_val(set, aff.get()));
  isl::val min_val = isl::manage(isl_set_min_val(set, aff.get()));

  return std::make_tuple(min_val, max_val);
}

isl::map isl_set_dim_name_if_null(isl_map *map, std::function<std::string(isl_dim_type, int)> namer) {
  int in_dims   = isl_map_dim(map, isl_dim_in);
  int out_dims  = isl_map_dim(map, isl_dim_out);
  auto set_name = [&](isl_dim_type dim_type) {
    for (int i = 0; i < isl_map_dim(map, dim_type); i++) {
      if (!isl_map_get_dim_name(map, dim_type, i)) {
        map = isl_map_set_dim_name(map, dim_type, i, namer(dim_type, i).c_str());
      }
    }
  };

  set_name(isl_dim_in);
  set_name(isl_dim_out);

  return isl::manage(map);
}

isl::set isl_set_dim_name_if_null(isl_set *set, std::function<std::string(isl_dim_type, int)> namer) {
  for (int i = 0; i < isl_set_dim(set, isl_dim_set); i++) {
    if (!isl_set_get_dim_name(set, isl_dim_set, i)) {
      set = isl_set_set_dim_name(set, isl_dim_set, i, namer(isl_dim_set, i).c_str());
    }
  }
  return isl::manage(set);
}

}  // namespace poly
}  // namespace cinn
