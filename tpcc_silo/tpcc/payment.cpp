#include "interface.h"
#include "index/masstree_beta/include/masstree_beta_wrapper.h"
#include "tpcc/tpcc_query.hpp"

using namespace ccbench;

namespace TPCC {
constexpr bool g_wh_update = true;

bool run_payment(query::Payment *query, HistoryKeyGenerator *hkg, Token &token) {
  Tuple *ret_tuple_ptr;
  Status stat;

  std::uint64_t w_id = query->w_id;
  std::uint64_t c_w_id = query->c_w_id;
#ifndef DBx1000_COMMENT_OUT
  std::uint64_t d_id = query->d_id;
  std::uint64_t c_id = query->c_id;
  std::uint64_t c_d_id = query->c_d_id;
#endif
  // ====================================================+
  // EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
  //   WHERE w_id=:w_id;
  // EXEC SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
  //   INTO :w_street_1, :w_street_2, :w_city, :w_state, :w_zip, :w_name
  //   FROM warehouse
  //   WHERE w_id=:w_id;
  // +===================================================================*/
  TPCC::Warehouse wh{};
  std::string wh_key = TPCC::Warehouse::CreateKey(w_id);
  stat = search_key(token, Storage::WAREHOUSE, wh_key, &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  memcpy(&wh, reinterpret_cast<void *>(const_cast<char *>(ret_tuple_ptr->get_val().data())), sizeof(TPCC::Warehouse));
  double w_ytd = wh.W_YTD;
  std::string w_name(wh.W_NAME);
  if (g_wh_update) {
    wh.W_YTD = w_ytd + query->h_amount;

    stat = update(token, Storage::WAREHOUSE, wh_key, {reinterpret_cast<char *>(&wh), sizeof(wh)},
                  static_cast<std::align_val_t>(alignof(TPCC::Warehouse)));
    if (stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }
  }

  // =====================================================+
  // EXEC SQL UPDATE district SET d_ytd = d_ytd + :h_amount
  //   WHERE d_w_id=:w_id AND d_id=:d_id;
  // EXEC SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
  //   INTO :d_street_1, :d_street_2, :d_city, :d_state, :d_zip, :d_name
  //   FROM district
  //   WHERE d_w_id=:w_id AND d_id=:d_id;
  // +====================================================================*/

  TPCC::District dist{};
  std::string dist_key = TPCC::District::CreateKey(w_id, d_id);
  stat = search_key(token, Storage::DISTRICT, dist_key, &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  memcpy(&dist, reinterpret_cast<void *>(const_cast<char *>(ret_tuple_ptr->get_val().data())), sizeof(TPCC::District));

  dist.D_YTD += query->h_amount;
  std::string d_name(dist.D_NAME);

  stat = update(token, Storage::DISTRICT, dist_key, {reinterpret_cast<char *>(&dist), sizeof(dist)},
                static_cast<std::align_val_t>(alignof(TPCC::District)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  TPCC::Customer cust{};
  std::string cust_key;
  if (query->by_last_name) {
    // ==========================================================
    // EXEC SQL SELECT count(c_id) INTO :namecnt
    //   FROM customer
    //   WHERE c_last=:c_last AND c_d_id=:c_d_id AND c_w_id=:c_w_id;
    // EXEC SQL DECLARE c_byname CURSOR FOR
    //   SELECT c_first, c_middle, c_id, c_street_1, c_street_2, c_city, c_state,
    //     c_zip, c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since
    //   FROM customer
    //   WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_last=:c_last
    //   ORDER BY c_first;
    // EXEC SQL OPEN c_byname;
    // if (namecnt%2) namecnt++; // Locate midpoint customer;
    // for (n=0; n<namecnt/2; n++) {
    //   EXEC SQL FETCH c_byname
    //     INTO :c_first, :c_middle, :c_id,
    //       :c_street_1, :c_street_2, :c_city, :c_state, :c_zip,
    //       :c_phone, :c_credit, :c_credit_lim, :c_discount, :c_balance, :c_since;
    // }
    // EXEC SQL CLOSE c_byname;
    // ==========================================================


    // TODO : to be implemented for CCBench


  } else { // search customers by cust_id
    // ==========================================================
    // EXEC SQL SELECT c_first, c_middle, c_last,
    //     c_street_1, c_street_2, c_city, c_state, c_zip,
    //     c_phone, c_credit, c_credit_lim,
    //     c_discount, c_balance, c_since
    //   INTO :c_first, :c_middle, :c_last,
    //     :c_street_1, :c_street_2, :c_city, :c_state, :c_zip,
    //     :c_phone, :c_credit, :c_credit_lim,
    //     :c_discount, :c_balance, :c_since
    //   FROM customer
    //   WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
    // ==========================================================
    cust_key = TPCC::Customer::CreateKey(w_id, d_id, c_id);
    stat = search_key(token, Storage::CUSTOMER, cust_key, &ret_tuple_ptr);
    if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }
    memcpy(&cust, reinterpret_cast<void *>(const_cast<char *>(ret_tuple_ptr->get_val().data())),
           sizeof(TPCC::Customer));
  }
  cust.C_BALANCE += query->h_amount;
  cust.C_YTD_PAYMENT += query->h_amount;
  cust.C_PAYMENT_CNT += 1;
  std::string c_credit(cust.C_CREDIT);

#ifndef DBx1000_COMMENT_OUT
  if (c_credit.find("BC") != std::string::npos) {
    // ==========================================================
    // EXEC SQL SELECT c_data INTO :c_data
    //   FROM customer
    //   WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
    // sprintf(c_new_data,"| %4d %2d %4d %2d %4d $%7.2f %12c %24c",
    //   c_id,c_d_id,c_w_id,d_id,w_id,h_amount
    //   h_date, h_data);
    // strncat(c_new_data,c_data,500-strlen(c_new_data));
    // ==========================================================
    char c_new_data[501];
    sprintf(c_new_data, "| %4ld %2ld %4ld %2ld %4ld $%7.2f",
            c_id, c_d_id, c_w_id, d_id, w_id, query->h_amount);
    std::string s1(c_new_data);
    std::string s2(cust.C_DATA, 500-s1.size());
    strncpy(cust.C_DATA, (s1+s2).c_str(), 500);
  }
  // ==========================================================
  // EXEC SQL UPDATE customer
  //   SET c_balance = :c_balance, c_data = :c_new_data
  //   WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND
  //     c_id = :c_id;
  // ==========================================================
  stat = update(token, Storage::CUSTOMER, cust_key, {reinterpret_cast<char *>(&cust), sizeof(cust)},
                static_cast<std::align_val_t>(alignof(TPCC::Customer)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  // ================================================================================
  // EXEC SQL INSERT INTO
  //   history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
  //   VALUES (:c_d_id, :c_w_id, :c_id, :d_id, :w_id, :datetime, :h_amount, :h_data);
  // ================================================================================
  TPCC::History hist{};
  hist.H_C_ID = c_id;
  hist.H_C_D_ID = c_d_id;
  hist.H_C_W_ID = c_w_id;
  hist.H_D_ID = d_id;
  hist.H_W_ID = w_id;
  hist.H_DATE = 2013;
  hist.H_AMOUNT = query->h_amount;
#if !TPCC_SMALL
  sprintf(hist.H_DATA, "%-10.10s    %.10s", w_name.c_str(), d_name.c_str());
#endif
  std::string hist_key = std::to_string(hkg->get());
  stat = insert(token, Storage::HISTORY, hist_key, {reinterpret_cast<char *>(&hist), sizeof(hist)},
                static_cast<std::align_val_t>(alignof(TPCC::History)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
#endif // DBx1000_COMMENT_OUT

  if (commit(token) == Status::OK) {
    return true;
  }
  abort(token);
  return false;
}
}
