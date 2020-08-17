/**
 * @file log.cpp
 * @brief implement about log
 */

#include "log.h"

namespace ccbench {

void Log::LogHeader::init() {
  checksum_ = 0;
  log_rec_num_ = 0;
}

void Log::LogHeader::compute_two_complement_of_checksum() {
  checksum_ ^= mask_full_bits_uint;
  ++checksum_;
}

unsigned int Log::LogHeader::get_checksum() const {  // NOLINT
  return checksum_;
}

void Log::LogHeader::add_checksum(const int add) { checksum_ += add; }

unsigned int Log::LogHeader::get_log_rec_num() const {  // NOLINT
  return log_rec_num_;
}

void Log::LogHeader::inc_log_rec_num() { ++this->log_rec_num_; }

void Log::LogHeader::set_checksum(unsigned int checksum) {
  this->checksum_ = checksum;
}

unsigned int Log::LogRecord::compute_checksum() {  // NOLINT
  // compute checksum
  // tid_word
  unsigned int chkSum = 0;
  const char *charitr = reinterpret_cast<char *>(this);  // NOLINT
  for (std::size_t i = 0; i < sizeof(tid_word); ++i) {
    chkSum += (*charitr);
    ++charitr;  // NOLINT
  }

  // OP_TYPE
  chkSum += static_cast<decltype(chkSum)>(op_);

  // key_length
  std::string_view key_view = tuple_->get_key();
  std::size_t key_length = key_view.size();
  charitr = reinterpret_cast<char *>(&(key_length));  // NOLINT
  for (std::size_t i = 0; i < sizeof(std::size_t); ++i) {
    chkSum += (*charitr);
    ++charitr;  // NOLINT
  }

  // key_body
  charitr = key_view.data();
  for (std::size_t i = 0; i < key_view.size(); ++i) {
    chkSum += (*charitr);
    ++charitr;  // NOLINT
  }

  // value_length
  std::string_view value_view = tuple_->get_val();
  std::size_t value_length = value_view.size();
  charitr = reinterpret_cast<char *>(&(value_length));  // NOLINT
  for (std::size_t i = 0; i < sizeof(std::size_t); ++i) {
    chkSum += (*charitr);
    ++charitr;  // NOLINT
  }

  // value_body
  charitr = value_view.data();
  for (std::size_t i = 0; i < value_view.size(); ++i) {
    chkSum += (*charitr);
    ++charitr;  // NOLINT
  }

  return chkSum;
}

[[maybe_unused]] void Log::single_recovery_from_log() {
  std::vector<LogRecord> log_set;
  for (auto i = 0; i < KVS_MAX_PARALLEL_THREADS; ++i) {
    File logfile{};
    std::string filename(kLogDirectory);
    filename.append("/log");
    filename.append(std::to_string(i));
    if (!logfile.try_open(filename, O_RDONLY)) {
      /**
       * the file doesn't exist.
       */
      continue;
    }

    LogRecord log{};
    LogHeader log_header{};
    std::vector<Tuple> tuple_buffer;

    const std::size_t fix_size = sizeof(tid_word) + sizeof(OP_TYPE);
    while (sizeof(LogHeader) ==
           logfile.read(reinterpret_cast<void *>(&log_header),  // NOLINT
                        sizeof(LogHeader))) {
      std::vector<LogRecord> log_tmp_buf;
      for (unsigned int j = 0; j < log_header.get_log_rec_num(); ++j) {
        if (fix_size != logfile.read(static_cast<void *>(&log), fix_size)) break;
        std::unique_ptr<char[]> key_ptr;    // NOLINT
        std::unique_ptr<char[]> value_ptr;  // NOLINT
        std::size_t key_length{};
        std::size_t value_length{};
        // read key_length
        if (sizeof(std::size_t) != logfile.read(static_cast<void *>(&key_length),
                                                sizeof(std::size_t))) {
          break;
        }
        // read key_body
        if (key_length > 0) {
          key_ptr = std::make_unique<char[]>(key_length);  // NOLINT
          if (key_length !=
              logfile.read(static_cast<void *>(key_ptr.get()), key_length)) {
            break;
          }
        }
        // read value_length
        if (sizeof(std::size_t) !=
            logfile.read(static_cast<void *>(&value_length),
                         sizeof(std::size_t))) {
          break;
        }
        // read value_body
        if (value_length > 0) {
          value_ptr = std::make_unique<char[]>(value_length);  // NOLINT
          if (value_length !=
              logfile.read(static_cast<void *>(value_ptr.get()), value_length)) {
            break;
          }
        }

        log_header.set_checksum(log_header.get_checksum() +
                                log.compute_checksum());
        log_tmp_buf.emplace_back(std::move(log));
      }
      if (log_header.get_checksum() == 0) {
        for (auto &&itr : log_tmp_buf) {
          log_set.emplace_back(std::move(itr));
        }
      } else {
        break;
      }
    }

    logfile.close();
  }

  /**
   * If no log files exist, it return.
   */
  if (log_set.empty()) return;

  sort(log_set.begin(), log_set.end());
  const epoch::epoch_t recovery_epoch =
          log_set.back().get_tid().get_epoch() - 2;

  Token s{};
  Storage st{};
  enter(s);
  for (auto &&itr : log_set) {
    if (itr.get_tid().get_epoch() > recovery_epoch) break;
    if (itr.get_op() == OP_TYPE::UPDATE || itr.get_op() == OP_TYPE::INSERT) {
      upsert(s, st, itr.get_tuple()->get_key(), itr.get_tuple()->get_val(), itr.get_tuple()->get_val_align());
    } else if (itr.get_op() == OP_TYPE::DELETE) {
      delete_record(s, st, itr.get_tuple()->get_key());
    }
    commit(s);
  }
  leave(s);
}

}  // namespace ccbench
