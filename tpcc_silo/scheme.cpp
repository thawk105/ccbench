/**
 * @file scheme.cpp
 * @brief about scheme
 */

#include "garbage_collection.h"
#include "session_info.h"

namespace ccbench {

bool write_set_obj::operator<(const write_set_obj &right) const {  // NOLINT
  Storage lhs_st = get_st();
  Storage rhs_st = right.get_st();
  if (lhs_st != rhs_st) return lhs_st < rhs_st;

  std::string_view lhs_key = get_tuple().get_key();
  std::string_view rhs_key = right.get_tuple().get_key();
  return lhs_key < rhs_key;
}

}  // namespace ccbench
