/**
 * @file payment.cpp
 */

#include "interface.h"
#include "index/masstree_beta/include/masstree_beta_wrapper.h"
#include "tpcc/tpcc_query.hpp"


using namespace ccbench;

namespace TPCC {
constexpr bool g_wh_update = true;

bool run_payment(query::Payment *query, HistoryKeyGenerator *hkg, Token &token) {
  Tuple *ret_tuple_ptr;
  Status stat;

  std::uint16_t w_id = query->w_id;
  std::uint16_t c_w_id = query->c_w_id;
#ifndef DBx1000_COMMENT_OUT
  std::uint8_t d_id = query->d_id;
  std::uint32_t c_id = query->c_id;
  std::uint8_t c_d_id = query->c_d_id;
#endif
  // ====================================================+
  // EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
  //   WHERE w_id=:w_id;
  // EXEC SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
  //   INTO :w_street_1, :w_street_2, :w_city, :w_state, :w_zip, :w_name
  //   FROM warehouse
  //   WHERE w_id=:w_id;
  // +===================================================================*/
  SimpleKey<8> wh_key;
  TPCC::Warehouse::CreateKey(w_id, wh_key.ptr());
  stat = search_key(token, Storage::WAREHOUSE, wh_key.view(), &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  HeapObject w_obj;
  w_obj.allocate<TPCC::Warehouse>();
  TPCC::Warehouse& wh = w_obj.ref();
  memcpy(&wh, ret_tuple_ptr->get_val().data(), sizeof(wh));
  double w_ytd = wh.W_YTD;
  if (g_wh_update) {
    wh.W_YTD = w_ytd + query->h_amount;

    stat = update(token, Storage::WAREHOUSE, Tuple(wh_key.view(), std::move(w_obj)));
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
  SimpleKey<8> dist_key;
  TPCC::District::CreateKey(w_id, d_id, dist_key.ptr());
  stat = search_key(token, Storage::DISTRICT, dist_key.view(), &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  HeapObject d_obj;
  d_obj.allocate<TPCC::District>();
  TPCC::District& dist = d_obj.ref();
  memcpy(&dist, ret_tuple_ptr->get_val().data(), sizeof(dist));

  dist.D_YTD += query->h_amount;

  stat = update(token, Storage::DISTRICT, Tuple(dist_key.view(), std::move(d_obj)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  SimpleKey<8> cust_key;
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
    char c_last_key_buf[Customer::CLastKey::required_size()];
    std::string_view c_last_key = Customer::CreateSecondaryKey(c_w_id, c_d_id, query->c_last, &c_last_key_buf[0]);
    void *ret_ptr = kohler_masstree::find_record(Storage::SECONDARY, c_last_key);
    assert(ret_ptr != nullptr);
    std::vector<SimpleKey<8>> *vec_ptr;
    std::string_view value_view = reinterpret_cast<Record *>(ret_ptr)->get_tuple().get_val();
    assert(value_view.size() == sizeof(uintptr_t));
    ::memcpy(&vec_ptr, value_view.data(), sizeof(uintptr_t));
    size_t nr_same_name = vec_ptr->size();
    assert(nr_same_name > 0);
    size_t idx = (nr_same_name + 1) / 2 - 1; // midpoint.
    cust_key = (*vec_ptr)[idx];
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
    TPCC::Customer::CreateKey(c_w_id, c_d_id, c_id, cust_key.ptr());
  }
  stat = search_key(token, Storage::CUSTOMER, cust_key.view(), &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  HeapObject c_obj;
  c_obj.allocate<TPCC::Customer>();
  TPCC::Customer& cust = c_obj.ref();
  memcpy(&cust, ret_tuple_ptr->get_val().data(), sizeof(cust));

  cust.C_BALANCE += query->h_amount;
  cust.C_YTD_PAYMENT += query->h_amount;
  cust.C_PAYMENT_CNT += 1;

#ifndef DBx1000_COMMENT_OUT
  if (cust.C_CREDIT[0] == 'B' && cust.C_CREDIT[1] == 'C') {
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
    size_t len = snprintf(&c_new_data[0], 501,
                          "| %4" PRIu32 " %2" PRIu8 " %4" PRIu16 " %2" PRIu16 " %4" PRIu16 " $%7.2f",
                          c_id, c_d_id, c_w_id, d_id, w_id, query->h_amount);
    assert(len <= 500);
#if 1
    size_t i = 0;
    while (len < 500) {
      char c = cust.C_DATA[i];
      if (c == '\0') break;
      c_new_data[len] = c;
      len++;
      i++;
    }
#else
    if (len < 500) {
        size_t len2 = snprintf(&c_new_data[len], 500 - len, "%s", &cust.C_DATA[0]);
        len2 = std::min(len2, 500 - len);
        // ::printf("len %zu len2 %zu total %zu\n", len, len2, len + len2);
        len += len2;
        assert(len <= 500);
    }
#endif
    ::memcpy(&cust.C_DATA[0], &c_new_data[0], len);
    cust.C_DATA[len] = '\0';
  }
  // ==========================================================
  // EXEC SQL UPDATE customer
  //   SET c_balance = :c_balance, c_data = :c_new_data
  //   WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND
  //     c_id = :c_id;
  // ==========================================================
  stat = update(token, Storage::CUSTOMER, Tuple(cust_key.view(), std::move(c_obj)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  // ================================================================================
  // EXEC SQL INSERT INTO
  //   history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
  //   VALUES (:c_d_id, :c_w_id, :c_id, :d_id, :w_id, :datetime, :h_amount, :h_data);
  // ================================================================================
  HeapObject h_obj;
  h_obj.allocate<TPCC::History>();
  TPCC::History& hist = h_obj.ref();
  hist.H_C_ID = c_id;
  hist.H_C_D_ID = c_d_id;
  hist.H_C_W_ID = c_w_id;
  hist.H_D_ID = d_id;
  hist.H_W_ID = w_id;
  hist.H_DATE = ccbench::epoch::get_lightweight_timestamp();
  hist.H_AMOUNT = query->h_amount;
#if !TPCC_SMALL
  sprintf(hist.H_DATA, "%-10.10s    %.10s", &wh.W_NAME[0], &dist.D_NAME[0]);
#endif
  SimpleKey<8> hist_key = hkg->get_as_simple_key();
  stat = insert(token, Storage::HISTORY, Tuple(hist_key.view(), std::move(h_obj)));
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
