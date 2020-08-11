/**
 * @file tuple.cpp
 * @brief about tuple
 */

#include "tuple.h"

#include "tuple_local.h"

namespace ccbench {

Tuple::Impl::Impl(const char *key_ptr, std::size_t key_length,
                  const char *value_ptr, std::size_t value_length)
        : pvalue_(new std::string(value_ptr, value_length)),  // NOLINT
          need_delete_pvalue_(true) {
  key_.assign(key_ptr, key_length);
}

Tuple::Impl::Impl(const Impl &right) : key_(right.key_) {
  if (right.need_delete_pvalue_) {
    this->need_delete_pvalue_ = true;
    this->pvalue_.store(new std::string(*right.pvalue_.load(  // NOLINT
            std::memory_order_acquire)),
                        std::memory_order_release);
  } else {
    this->need_delete_pvalue_ = false;
    this->pvalue_.store(nullptr, std::memory_order_release);
  }
}

Tuple::Impl::Impl(Impl &&right) {  // NOLINT
  this->key_ = right.key_;
  if (right.need_delete_pvalue_) {
    this->need_delete_pvalue_ = true;
    this->pvalue_.store(right.pvalue_.load(std::memory_order_acquire),
                        std::memory_order_release);
    right.need_delete_pvalue_ = false;
  } else {
    this->need_delete_pvalue_ = false;
    this->pvalue_.store(nullptr, std::memory_order_release);
  }
}

Tuple::Impl &Tuple::Impl::operator=(const Impl &right) {  // NOLINT
  // process about this
  if (this->need_delete_pvalue_) {
    delete this->pvalue_.load(std::memory_order_acquire);  // NOLINT
  }
  // process about copy assign
  this->key_ = right.key_;
  if (right.need_delete_pvalue_) {
    this->need_delete_pvalue_ = true;
    this->pvalue_.store(new std::string(*right.pvalue_.load(  // NOLINT
            std::memory_order_acquire)),
                        std::memory_order_release);
  } else {
    this->need_delete_pvalue_ = false;
    this->pvalue_.store(nullptr, std::memory_order_release);
  }

  return *this;
}

Tuple::Impl &Tuple::Impl::operator=(Impl &&right) {  // NOLINT
  // process about this
  if (this->need_delete_pvalue_) {
    delete this->pvalue_.load(std::memory_order_acquire);  // NOLINT
  }
  // process about move assign
  this->key_ = right.key_;
  if (right.need_delete_pvalue_) {
    this->need_delete_pvalue_ = true;
    this->pvalue_.store(right.pvalue_.load(std::memory_order_acquire),
                        std::memory_order_release);
    right.need_delete_pvalue_ = false;
  } else {
    this->need_delete_pvalue_ = false;
    this->pvalue_.store(nullptr, std::memory_order_release);
  }

  return *this;
}

[[nodiscard]] std::string_view Tuple::Impl::get_key() const {  // NOLINT
  return std::string_view{key_.data(), key_.size()};
}

[[nodiscard]] std::string_view Tuple::Impl::get_value() const {  // NOLINT
  if (need_delete_pvalue_) {
    // common subexpression elimination
    std::string *value = pvalue_.load(std::memory_order_acquire);
    return std::string_view{value->data(), value->size()};
  }
  return {};
}

void Tuple::Impl::reset() {
  if (need_delete_pvalue_) {
    delete pvalue_.load(std::memory_order_acquire);  // NOLINT
  }
}

void Tuple::Impl::set(const char *key_ptr, std::size_t key_length,
                      const char *value_ptr, std::size_t value_length) {
  key_.assign(key_ptr, key_length);
  if (need_delete_pvalue_) {
    delete pvalue_.load(std::memory_order_acquire);  // NOLINT
  }

  pvalue_.store(new std::string(value_ptr, value_length),  // NOLINT
                std::memory_order_release);
  this->need_delete_pvalue_ = true;
}

[[maybe_unused]] void Tuple::Impl::set_key(const char *key_ptr,
                                           std::size_t key_length) {
  key_.assign(key_ptr, key_length);
}

void Tuple::Impl::set_value(const char *value_ptr, std::size_t value_length) {
  if (this->need_delete_pvalue_) {
    pvalue_.load(std::memory_order_acquire)->assign(value_ptr, value_length);
  } else {
    pvalue_.store(new std::string(value_ptr, value_length),  // NOLINT
                  std::memory_order_release);
    need_delete_pvalue_ = true;
  }
}

void Tuple::Impl::set_value(const char *value_ptr, std::size_t value_length,
                            std::string **old_value) {
  if (this->need_delete_pvalue_) {
    *old_value = pvalue_.load(std::memory_order_acquire);
  } else {
    *old_value = nullptr;
  }

  pvalue_.store(new std::string(value_ptr, value_length),  // NOLINT
                std::memory_order_release);
  this->need_delete_pvalue_ = true;
}

Tuple::Tuple() : pimpl_(std::make_unique<Impl>()) {}

Tuple::Tuple(const char *key_ptr, std::size_t key_length, const char *val_ptr,
             std::size_t val_length)
        : pimpl_(std::make_unique<Impl>(key_ptr, key_length, val_ptr, val_length)) {
}

Tuple::Tuple(const Tuple &right) {
  pimpl_ = std::make_unique<Impl>(*right.pimpl_);
}

Tuple::Tuple(Tuple &&right) { pimpl_ = std::move(right.pimpl_); }

Tuple &Tuple::operator=(const Tuple &right) {  // NOLINT
  this->pimpl_ = std::make_unique<Impl>(*right.pimpl_);

  return *this;
}

Tuple &Tuple::operator=(Tuple &&right) {  // NOLINT
  this->pimpl_ = std::move(right.pimpl_);

  return *this;
}

std::string_view Tuple::get_key() const {  // NOLINT
  return pimpl_->get_key();
}

std::string_view Tuple::get_value() const {  // NOLINT
  return pimpl_->get_value();
}

Tuple::Impl *Tuple::get_pimpl() { return pimpl_.get(); }  // NOLINT

}  // namespace ccbench
