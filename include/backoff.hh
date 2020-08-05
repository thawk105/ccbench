#pragma once

#include <x86intrin.h>

#include <atomic>
#include <cmath>
#include <iostream>

#include "atomic_wrapper.hh"
#include "result.hh"
#include "tsc.hh"
#include "util.hh"

using namespace std;

class Backoff {
public:
  static std::atomic<double> Backoff_;
  static constexpr double kMinBackoff = 0;
  static constexpr double kMaxBackoff = 1000;
  static constexpr double kIncrBackoff = 100;
  //static constexpr double kIncrBackoff = 0.5;

  uint64_t last_committed_txs_ = 0;
  double last_committed_tput_ = 0;
  uint64_t last_backoff_ = 0;
  uint64_t last_time_ = 0;
  size_t clocks_per_us_;

  Backoff(size_t clocks_per_us) {
    init(clocks_per_us);
  }

  void init(size_t clocks_per_us) {
    last_time_ = rdtscp();
    clocks_per_us_ = clocks_per_us;
  }

  bool check_update_backoff() {
    if (chkClkSpan(last_time_, rdtscp(), clocks_per_us_ * 10))
      return true;
    else
      return false;
  }

  void update_backoff(const uint64_t committed_txs) {
    uint64_t now = rdtscp();
    uint64_t time_diff = now - last_time_;
    last_time_ = now;

    double new_backoff = Backoff_.load(std::memory_order_acquire);
    double backoff_diff = new_backoff - last_backoff_;

    uint64_t committed_diff = committed_txs - last_committed_txs_;
    double committed_tput = static_cast<double>(committed_diff) /
                            (static_cast<double>(time_diff) / clocks_per_us_) *
                            pow(10.0, 6);
    double committed_tput_diff = committed_tput - last_committed_tput_;

    last_committed_txs_ = committed_txs;
    last_committed_tput_ = committed_tput;
    last_backoff_ = new_backoff;
    /*
    cout << "=====" << endl;
    cout << "committed_tput_diff:\t" <<
    static_cast<int64_t>(committed_tput_diff) << endl; cout <<
    "last_backoff_:\t" << last_backoff_ << endl; cout << "backoff_diff:\t" <<
    backoff_diff << endl;
    */

    double gradient;
    if (backoff_diff != 0)
      gradient = committed_tput_diff / backoff_diff;
    else
      gradient = 0;

    if (gradient < 0)
      new_backoff -= kIncrBackoff;
    else if (gradient > 0)
      new_backoff += kIncrBackoff;
    else {
      if ((committed_txs & 1) == 0 ||
          new_backoff == kMaxBackoff)  // 確率はおよそ 1/2, すなわちランダム．
        new_backoff -= kIncrBackoff;
      else if ((committed_txs & 1) == 1 || new_backoff == kMinBackoff)
        new_backoff += kIncrBackoff;
    }

    if (new_backoff < kMinBackoff)
      new_backoff = kMinBackoff;
    else if (new_backoff > kMaxBackoff)
      new_backoff = kMaxBackoff;
    Backoff_.store(new_backoff, std::memory_order_release);
  }

  static void backoff(size_t clocks_per_us) {
    uint64_t start(rdtscp()), stop;
    double now_backoff = Backoff_.load(std::memory_order_acquire);

    // printf("clocks_per_us * now_backoff:\t%lu\n",
    // static_cast<uint64_t>(static_cast<double>(clocks_per_us) * now_backoff));
    for (;;) {
      _mm_pause();
      stop = rdtscp();
      if (chkClkSpan(start, stop,
                     static_cast<uint64_t>(static_cast<double>(clocks_per_us) *
                                           now_backoff)))
        break;
    }
  }
};

[[maybe_unused]] inline void
leaderBackoffWork([[maybe_unused]] Backoff &backoff, [[maybe_unused]] std::vector <Result> &res) {
  if (backoff.check_update_backoff()) {
    uint64_t sum_committed_txs(0);
    for (auto &th : res) {
      sum_committed_txs += loadAcquire(th.local_commit_counts_);
    }
    backoff.update_backoff(sum_committed_txs);
  }
}

#ifdef GLOBAL_VALUE_DEFINE
std::atomic<double> Backoff::Backoff_(0);
#endif
