/**
 * @file scheme.cpp
 * @brief about scheme
 */

#include "garbage_collection.h"
#include "session_info.h"

namespace ccbench {

bool write_set_obj::operator<(const write_set_obj &right) const {  // NOLINT
  const Tuple &this_tuple = this->get_tuple(this->get_op());
  const Tuple &right_tuple = right.get_tuple(right.get_op());

  const char *this_key_ptr(this_tuple.get_key().data());
  const char *right_key_ptr(right_tuple.get_key().data());
  std::size_t this_key_size(this_tuple.get_key().size());
  std::size_t right_key_size(right_tuple.get_key().size());

  if (this_key_size < right_key_size) {
    return memcmp(this_key_ptr, right_key_ptr, this_key_size) <= 0;
  }

  if (this_key_size > right_key_size) {
    return memcmp(this_key_ptr, right_key_ptr, right_key_size) < 0;
  }
  int ret = memcmp(this_key_ptr, right_key_ptr, this_key_size);
  if (ret < 0) {
    return true;
  }
  if (ret > 0) {
    return false;
  }
  std::cout << __FILE__ << " : " << __LINE__
            << " : Unique key is not allowed now." << std::endl;
  std::abort();
}

void write_set_obj::reset_tuple_value(std::string_view val) {
  (this->get_op() == OP_TYPE::UPDATE ? this->get_tuple_to_local()
                                     : this->get_tuple_to_db())
          .set_value(val.data(), val.size());
}

}  // namespace ccbench
