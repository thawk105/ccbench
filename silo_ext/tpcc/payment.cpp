#include "interface.h"
#include "index/masstree_beta/include/masstree_beta_wrapper.h"
#include "tpcc/tpcc_query.hpp"

using namespace ccbench;

namespace TPCC {
constexpr bool g_wh_update = true;

bool run_payment(query::Payment *query, HistoryKeyGenerator *hkg) {
#ifdef DBx1000
  RC rc = RCOK;
  uint64_t key;
  itemid_t * item;
#else // CCBench
  TPCC::Warehouse *wh;
  std::string strkey;
  Tuple *ret_tuple_ptr;
  Status stat;
  Token token{};
  enter(token);
#endif

  uint64_t w_id = query->w_id;
  uint64_t c_w_id = query->c_w_id;
#ifndef DBx1000_COMMENT_OUT
  uint64_t d_id = query->d_id;
  uint64_t c_id = query->c_id;
  uint64_t c_d_id = query->c_d_id;
#endif
  // ====================================================+
  // EXEC SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
  //   WHERE w_id=:w_id;
  // +====================================================*/
  // ===================================================================+
  // EXEC SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name
  //   INTO :w_street_1, :w_street_2, :w_city, :w_state, :w_zip, :w_name
  //   FROM warehouse
  //   WHERE w_id=:w_id;
  // +===================================================================*/

#ifdef DBx1000
  // TODO for variable length variable (string). Should store the size of
  // the variable.
  key = query->w_id;
  INDEX * index = _wl->i_warehouse;
  item = index_read(index, key, wh_to_part(w_id));
  assert(item != NULL);
  row_t * r_wh = ((row_t *)item->location);
  row_t * r_wh_local;
  if (g_wh_update)
    r_wh_local = get_row(r_wh, WR);
  else
    r_wh_local = get_row(r_wh, RD);

  if (r_wh_local == NULL) {
    return finish(Abort);
  }
#else // CCBench
  strkey = TPCC::Warehouse::CreateKey(w_id);
  stat = search_key(token, Storage::WAREHOUSE, strkey, &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    leave(token);
    return false;
  }
  wh = (TPCC::Warehouse *) ret_tuple_ptr->get_val().data();
#endif

  double w_ytd;

  // =====================================================+
  // EXEC SQL UPDATE district SET d_ytd = d_ytd + :h_amount
  // WHERE d_w_id=:w_id AND d_id=:d_id;
  // +=====================================================*/
#ifdef DBx1000
  r_wh_local->get_value(W_YTD, w_ytd);
  if (g_wh_update) {
    r_wh_local->set_value(W_YTD, w_ytd + query->h_amount);
  }
  char w_name[11];
  char * tmp_str = r_wh_local->get_value(W_NAME);
  memcpy(w_name, tmp_str, 10);
  w_name[10] = '\0';
#else // CCBench
  w_ytd = wh->W_YTD;
  if (g_wh_update) {
    wh->W_YTD = w_ytd + query->h_amount;
  }
  std::string w_name(wh->W_NAME);
#endif

  // ====================================================================+
  // EXEC SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name
  //   INTO :d_street_1, :d_street_2, :d_city, :d_state, :d_zip, :d_name
  //   FROM district
  //   WHERE d_w_id=:w_id AND d_id=:d_id;
  // +====================================================================*/
#ifdef DBx1000
  key = distKey(query->d_id, query->d_w_id);
  item = index_read(_wl->i_district, key, wh_to_part(w_id));
  assert(item != NULL);
  row_t * r_dist = ((row_t *)item->location);
  row_t * r_dist_local = get_row(r_dist, WR);
  if (r_dist_local == NULL) {
    return finish(Abort);
  }

  double d_ytd;
  r_dist_local->get_value(D_YTD, d_ytd);
  r_dist_local->set_value(D_YTD, d_ytd + query->h_amount);
  char d_name[11];
  tmp_str = r_dist_local->get_value(D_NAME);
  memcpy(d_name, tmp_str, 10);
  d_name[10] = '\0';

  row_t * r_cust;
#else // CCBench
  TPCC::District *dist;
  strkey = TPCC::District::CreateKey(w_id, d_id);
  stat = search_key(token, Storage::DISTRICT, strkey, &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    leave(token);
    return false;
  }
  dist = (TPCC::District *) ret_tuple_ptr->get_val().data();

  dist->D_YTD += query->h_amount;
  std::string d_name(dist->D_NAME);

  TPCC::Customer *cust;
#endif

  if (query->by_last_name) {
    // ==========================================================+
    // EXEC SQL SELECT count(c_id) INTO :namecnt
    //   FROM customer
    //   WHERE c_last=:c_last AND c_d_id=:c_d_id AND c_w_id=:c_w_id;
    // +==========================================================*/
    // ==========================================================================+
    // EXEC SQL DECLARE c_byname CURSOR FOR
    //   SELECT c_first, c_middle, c_id, c_street_1, c_street_2, c_city, c_state,
    //     c_zip, c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since
    //   FROM customer
    //   WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_last=:c_last
    //   ORDER BY c_first;
    // EXEC SQL OPEN c_byname;
    // +===========================================================================*/
    // ============================================================================+
    // for (n=0; n<namecnt/2; n++) {
    //   EXEC SQL FETCH c_byname
    //     INTO :c_first, :c_middle, :c_id,
    //       :c_street_1, :c_street_2, :c_city, :c_state, :c_zip,
    //       :c_phone, :c_credit, :c_credit_lim, :c_discount, :c_balance, :c_since;
    // }
    // EXEC SQL CLOSE c_byname;
    // +=============================================================================*/
    // XXX: we don't retrieve all the info, just the tuple we are interested in
#ifdef DBx1000
    uint64_t key = custNPKey(query->c_last, query->c_d_id, query->c_w_id);
    // XXX: the list is not sorted. But let's assume it's sorted...
    // The performance won't be much different.
    INDEX * index = _wl->i_customer_last;
    item = index_read(index, key, wh_to_part(c_w_id));
    assert(item != NULL);

    int cnt = 0;
    itemid_t * it = item;
    itemid_t * mid = item;
    while (it != NULL) {
      cnt ++;
      it = it->next;
      if (cnt % 2 == 0)
        mid = mid->next;
    }
    r_cust = ((row_t *)mid->location);

#else // CCBench

    // to be implemented for CCBench

#endif
  } else { // search customers by cust_id
    // =====================================================================+
    // EXEC SQL SELECT c_first, c_middle, c_last, c_street_1, c_street_2,
    //     c_city, c_state, c_zip, c_phone, c_credit, c_credit_lim,
    //     c_discount, c_balance, c_since
    //   INTO :c_first, :c_middle, :c_last, :c_street_1, :c_street_2,
    //     :c_city, :c_state, :c_zip, :c_phone, :c_credit, :c_credit_lim,
    //     :c_discount, :c_balance, :c_since
    //   FROM customer
    //   WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
    // +======================================================================*/
#ifdef DBx1000
    key = custKey(query->c_id, query->c_d_id, query->c_w_id);
    INDEX * index = _wl->i_customer_id;
    item = index_read(index, key, wh_to_part(c_w_id));
    assert(item != NULL);
    r_cust = (row_t *) item->location;
#else // CCBench
    strkey = TPCC::Customer::CreateKey(w_id, d_id, c_id);
    stat = search_key(token, Storage::CUSTOMER, strkey, &ret_tuple_ptr);
    if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
      abort(token);
      leave(token);
      return false;
    }
    cust = reinterpret_cast<TPCC::Customer *>(const_cast<char *>(ret_tuple_ptr->get_val().data()));
#endif
  }

  // ======================================================================+
  // EXEC SQL UPDATE customer SET c_balance = :c_balance, c_data = :c_new_data
  //   WHERE c_w_id = :c_w_id AND c_d_id = :c_d_id AND c_id = :c_id;
  // +======================================================================*/
#ifdef DBx1000
  row_t * r_cust_local = get_row(r_cust, WR);
  if (r_cust_local == NULL) {
    return finish(Abort);
  }
  double c_balance;
  double c_ytd_payment;
  double c_payment_cnt;

  r_cust_local->get_value(C_BALANCE, c_balance);
  r_cust_local->set_value(C_BALANCE, c_balance - query->h_amount);
  r_cust_local->get_value(C_YTD_PAYMENT, c_ytd_payment);
  r_cust_local->set_value(C_YTD_PAYMENT, c_ytd_payment + query->h_amount);
  r_cust_local->get_value(C_PAYMENT_CNT, c_payment_cnt);
  r_cust_local->set_value(C_PAYMENT_CNT, c_payment_cnt + 1);

  char * c_credit = r_cust_local->get_value(C_CREDIT);
#else // CCBench
  cust->C_BALANCE += query->h_amount;
  cust->C_YTD_PAYMENT += query->h_amount;
  cust->C_PAYMENT_CNT += 1;
  std::string c_credit(cust->C_CREDIT);
#endif

#ifndef DBx1000_COMMENT_OUT
  // =====================================================+
  // EXEC SQL SELECT c_data
  //   INTO :c_data
  //   FROM customer
  //   WHERE c_w_id=:c_w_id AND c_d_id=:c_d_id AND c_id=:c_id;
  // +=====================================================*/
#ifdef DBx1000
  if ( strstr(c_credit, "BC") ) {
    char c_new_data[501];
    sprintf(c_new_data,"| %4ld %2ld %4ld %2ld %4ld $%7.2f",
            c_id, c_d_id, c_w_id, d_id, w_id, query->h_amount);
    char k_c_data[] = "C_DATA";
    char * c_data = r_cust->get_value(k_c_data);
    strncat(c_new_data, c_data, 500 - strlen(c_new_data));
    r_cust->set_value("C_DATA", c_new_data);
  }
#else // CCBench
  if (c_credit.find("BC") != std::string::npos) {
    char c_new_data[501];
    sprintf(c_new_data, "| %4ld %2ld %4ld %2ld %4ld $%7.2f",
            c_id, c_d_id, c_w_id, d_id, w_id, query->h_amount);
    strncat(c_new_data, cust->C_DATA, 500 - strlen(c_new_data));
    strncpy(cust->C_DATA, c_new_data, 501); // update?
  }
#endif
#endif // DBx1000_COMMENT_OUT

  // =============================================================================+
  // EXEC SQL INSERT INTO
  //   history (h_c_d_id, h_c_w_id, h_c_id, h_d_id, h_w_id, h_date, h_amount, h_data)
  //   VALUES (:c_d_id, :c_w_id, :c_id, :d_id, :w_id, :datetime, :h_amount, :h_data);
  // +=============================================================================*/
#ifndef DBx1000_COMMENT_OUT
#ifdef DBx1000
  char h_data[25];
  strncpy(h_data, w_name, 10);
  int length = strlen(h_data);
  if (length > 10) length = 10;
  strcpy(&h_data[length], "    ");
  strncpy(&h_data[length + 4], d_name, 10);
  h_data[length+14] = '\0';

  row_t * r_hist;
  uint64_t row_id;
  _wl->t_history->get_new_row(r_hist, 0, row_id);
  r_hist->set_value(H_C_ID, c_id);
  r_hist->set_value(H_C_D_ID, c_d_id);
  r_hist->set_value(H_C_W_ID, c_w_id);
  r_hist->set_value(H_D_ID, d_id);
  r_hist->set_value(H_W_ID, w_id);
  int64_t date = 2013;
  r_hist->set_value(H_DATE, date);
  r_hist->set_value(H_AMOUNT, query->h_amount);
#if !TPCC_SMALL
  r_hist->set_value(H_DATA, h_data);
#endif
  insert_row(r_hist, _wl->t_history);
#else // CCBench
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
  strkey = std::to_string(hkg->get());
  stat = insert(token, Storage::HISTORY, strkey, {reinterpret_cast<char *>(&hist), sizeof(hist)}, alignof(TPCC::History));
#endif
#endif // DBx1000_COMMENT_OUT

  if (commit(token) == Status::OK) {
    leave(token);
    return true;
  }
  abort(token);
  leave(token);
  return false;
}
}
