#include "interface.h"
#include "masstree_beta_wrapper.h"
#include "tpcc/tpcc_query.hpp"

using namespace ccbench;

namespace TPCC {

#ifdef DBx1000
#if TPCC_SMALL
const uint32_t g_max_items = 10000;
const int32_t g_cust_per_dist = 2000;
#else
const uint32_t g_max_items = 100000;
const uint32_t g_cust_per_dist = 3000;
#endif

//#define DIST_PER_WARE (10)

// Helper Functions
uint64_t distKey(uint64_t d_id, uint64_t d_w_id) {
  return d_w_id * DIST_PER_WARE + d_id;
}

uint64_t custKey(uint64_t c_id, uint64_t c_d_id, uint64_t c_w_id) {
  return (distKey(c_d_id, c_w_id) * g_cust_per_dist + c_id);
}

uint64_t stockKey(uint64_t s_i_id, uint64_t s_w_id) {
  return s_w_id * g_max_items + s_i_id;
}
#endif

bool
run_new_order(TPCC::query::NewOrder *query, Token& token) {
  //itemid_t * item;
  //INDEX * index;

  bool remote = query->remote;
  uint64_t w_id = query->w_id;
  uint64_t d_id = query->d_id;
  uint64_t c_id = query->c_id;
  uint64_t ol_cnt = query->ol_cnt;

  /*=======================================================================+
    EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
    INTO :c_discount, :c_last, :c_credit, :w_tax
    FROM customer, warehouse
    WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
    +========================================================================*/
  TPCC::Warehouse *wh;
  std::string wh_key;
  wh_key = TPCC::Warehouse::CreateKey(w_id);
  // index = _wl->i_warehouse;
  // item = index_read(index, key, wh_to_part(w_id));
  Tuple *ret_tuple_ptr;
  Status stat;
  stat = search_key(token, Storage::WAREHOUSE, wh_key, &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  wh = (TPCC::Warehouse *) ret_tuple_ptr->get_val().data();

  [[maybe_unused]] double w_tax = wh->W_TAX;
  //uint64_t key = custKey(c_id, d_id, w_id);
  TPCC::Customer *cust;
  std::string cust_key = TPCC::Customer::CreateKey(w_id, d_id, c_id);
  stat = search_key(token, Storage::CUSTOMER, cust_key, &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  cust = reinterpret_cast<TPCC::Customer *>(const_cast<char *>(ret_tuple_ptr->get_val().data()));
  double c_discount = cust->C_DISCOUNT;
  [[maybe_unused]] char *c_last = cust->C_LAST;
  [[maybe_unused]] char *c_credit = cust->C_CREDIT;

#ifdef DBx1000
  key = w_id;
  index = _wl->i_warehouse;
  item = index_read(index, key, wh_to_part(w_id));
  assert(item != NULL);
  row_t * r_wh = ((row_t *)item->location);
  row_t * r_wh_local = get_row(r_wh, RD);
  if (r_wh_local == NULL) {
    return finish(Abort);
  }


  double w_tax;
  r_wh_local->get_value(W_TAX, w_tax);
  key = custKey(c_id, d_id, w_id);
  index = _wl->i_customer_id;
  item = index_read(index, key, wh_to_part(w_id));
  assert(item != NULL);
  row_t * r_cust = (row_t *) item->location;
  row_t * r_cust_local = get_row(r_cust, RD);
  if (r_cust_local == NULL) {
    return finish(Abort);
  }
  uint64_t c_discount;
  //char * c_last;
  //char * c_credit;
  r_cust_local->get_value(C_DISCOUNT, c_discount);
  //c_last = r_cust_local->get_value(C_LAST);
  //c_credit = r_cust_local->get_value(C_CREDIT);
#endif

  /*==================================================+
    EXEC SQL SELECT d_next_o_id, d_tax
    INTO :d_next_o_id, :d_tax
    FROM district WHERE d_id = :d_id AND d_w_id = :w_id;

    EXEC SQL UPDATE district SET d_next_o_id = :d_next_o_id + 1
    WHERE d_id = :d_id AND d_w_id = :w_id ;
    +===================================================*/
  TPCC::District dist;
  std::string dist_key = TPCC::District::CreateKey(w_id, d_id);
  stat = search_key(token, Storage::DISTRICT, dist_key, &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  memcpy(&dist, reinterpret_cast<void*>(const_cast<char *>(ret_tuple_ptr->get_val().data())), sizeof(TPCC::District));

  [[maybe_unused]] double d_tax = dist.D_TAX;
  std::uint32_t o_id = dist.D_NEXT_O_ID;
  o_id++;
  dist.D_NEXT_O_ID = o_id;
  // o_id = dist.D_NEXT_O_ID; /* no need to execute */
  // TODO : tanabe will write below
  // update(token, Storage::DISTRICT, dist_key, dist, alignof(TPCC::District));

#ifdef DBx1000
  key = distKey(d_id, w_id);
  item = index_read(_wl->i_district, key, wh_to_part(w_id));
  assert(item != NULL);
  row_t * r_dist = ((row_t *)item->location);
  row_t * r_dist_local = get_row(r_dist, WR);
  if (r_dist_local == NULL) {
    return finish(Abort);
  }
  //double d_tax;
  int64_t o_id;
  //d_tax = *(double *) r_dist_local->get_value(D_TAX);
  o_id = *(int64_t *) r_dist_local->get_value(D_NEXT_O_ID);
  o_id ++;
  r_dist_local->set_value(D_NEXT_O_ID, o_id);
#endif


  /*========================================================================================+
    EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
    VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
    +========================================================================================*/
  TPCC::Order order{};
  std::uint64_t o_entry_d{}; // dummy data. Is it necessary?

  order.O_ID = o_id;
  order.O_D_ID = d_id;
  order.O_W_ID = w_id;
  order.O_C_ID = c_id;
  order.O_ENTRY_D = o_entry_d;
  order.O_OL_CNT = ol_cnt;
  int64_t all_local = (remote ? 0 : 1);
  order.O_ALL_LOCAL = all_local;
  std::string order_key = TPCC::Order::CreateKey(order.O_W_ID, order.O_D_ID, order.O_ID);
  /**
   * TODO : check. Is it ok either insert operation successes or not.
   */
  insert(token, Storage::ORDER, order_key, {(char *) &order, sizeof(TPCC::Order)}, alignof(TPCC::Order));

#ifdef DBx1000
  row_t * r_order;
  uint64_t row_id;
  _wl->t_order->get_new_row(r_order, 0, row_id);
  r_order->set_value(O_ID, o_id);
  r_order->set_value(O_C_ID, c_id);
  r_order->set_value(O_D_ID, d_id);
  r_order->set_value(O_W_ID, w_id);
  r_order->set_value(O_ENTRY_D, o_entry_d);
  r_order->set_value(O_OL_CNT, ol_cnt);
  int64_t all_local = (remote? 0 : 1);
  r_order->set_value(O_ALL_LOCAL, all_local);
  insert_row(r_order, _wl->t_order);
#endif

  /*=======================================================+
    EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
    VALUES (:o_id, :d_id, :w_id);
    +=======================================================*/

  TPCC::NewOrder neworder{};
  neworder.NO_O_ID = o_id;
  neworder.NO_D_ID = d_id;
  neworder.NO_W_ID = w_id;
  std::string no_key = TPCC::NewOrder::CreateKey(neworder.NO_W_ID, neworder.NO_D_ID, neworder.NO_O_ID);
  /**
   * TODO : check. Is it ok either insert operation successes or not.
   */
  insert(token, Storage::NEWORDER, no_key, {(char *) &neworder, sizeof(TPCC::NewOrder)}, alignof(TPCC::NewOrder));

#ifdef DBx1000
  row_t * r_no;
  _wl->t_neworder->get_new_row(r_no, 0, row_id);
  r_no->set_value(NO_O_ID, o_id);
  r_no->set_value(NO_D_ID, d_id);
  r_no->set_value(NO_W_ID, w_id);
  insert_row(r_no, _wl->t_neworder);
#endif

  for (uint32_t ol_number = 0; ol_number < ol_cnt; ol_number++) {
    /*===========================================+
      EXEC SQL SELECT i_price, i_name , i_data
      INTO :i_price, :i_name, :i_data
      FROM item
      WHERE i_id = :ol_i_id;
      +===========================================*/

    uint64_t ol_i_id = query->items[ol_number].ol_i_id;
    uint64_t ol_supply_w_id = query->items[ol_number].ol_supply_w_id;
    uint64_t ol_quantity = query->items[ol_number].ol_quantity;

    /*
     * ### Unnecessary? ###
     * price[ol_number-1] = i_price;
     * strncpy(iname[ol_number-1],i_name,24);
     */

    TPCC::Item *item;
    std::string item_key = TPCC::Item::CreateKey(ol_i_id);
    stat = search_key(token, Storage::ITEM, item_key, &ret_tuple_ptr);
    if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }
    item = (TPCC::Item *) ret_tuple_ptr->get_val().data();
    double i_price = item->I_PRICE;
    [[maybe_unused]] char *i_name = item->I_NAME;
    [[maybe_unused]] char *i_data = item->I_DATA;

#ifdef DBx1000
    uint64_t ol_i_id = query->items[ol_number].ol_i_id;
    uint64_t ol_supply_w_id = query->items[ol_number].ol_supply_w_id;
    uint64_t ol_quantity = query->items[ol_number].ol_quantity;
    key = ol_i_id;
    item = index_read(_wl->i_item, key, 0);
    assert(item != NULL);
    row_t * r_item = ((row_t *)item->location);
    row_t * r_item_local = get_row(r_item, RD);
    if (r_item_local == NULL) {
      return finish(Abort);
    }
    int64_t i_price;
    char * i_name;
    char * i_data;
    r_item_local->get_value(I_PRICE, i_price);
    i_name = r_item_local->get_value(I_NAME);
    i_data = r_item_local->get_value(I_DATA);
#endif

    /*===================================================================+
      EXEC SQL SELECT s_quantity, s_data,
      s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05,
      s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10
      INTO :s_quantity, :s_data,
      :s_dist_01, :s_dist_02, :s_dist_03, :s_dist_04, :s_dist_05,
      :s_dist_06, :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10
      FROM stock
      WHERE s_i_id = :ol_i_id AND s_w_id = :ol_supply_w_id;

      EXEC SQL UPDATE stock SET s_quantity = :s_quantity
      WHERE s_i_id = :ol_i_id
      AND s_w_id = :ol_supply_w_id;
      +===============================================*/

    TPCC::Stock stock;
    std::string stock_key = TPCC::Stock::CreateKey(ol_supply_w_id, ol_i_id);
    stat = search_key(token, Storage::STOCK, stock_key, &ret_tuple_ptr);
    if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }
    memcpy(&stock, reinterpret_cast<void*>(const_cast<char *>(ret_tuple_ptr->get_val().data())), sizeof(TPCC::Stock));
    uint64_t s_quantity = (int64_t) stock.S_QUANTITY;
    /**
     * These data are for application side.
     */
    [[maybe_unused]] char *s_data = stock.S_DATA;
    [[maybe_unused]] char *s_dist_01 = stock.S_DIST_01;
    [[maybe_unused]] char *s_dist_02 = stock.S_DIST_02;
    [[maybe_unused]] char *s_dist_03 = stock.S_DIST_03;
    [[maybe_unused]] char *s_dist_04 = stock.S_DIST_04;
    [[maybe_unused]] char *s_dist_05 = stock.S_DIST_05;
    [[maybe_unused]] char *s_dist_06 = stock.S_DIST_06;
    [[maybe_unused]] char *s_dist_07 = stock.S_DIST_07;
    [[maybe_unused]] char *s_dist_08 = stock.S_DIST_08;
    [[maybe_unused]] char *s_dist_09 = stock.S_DIST_09;
    [[maybe_unused]] char *s_dist_10 = stock.S_DIST_10;

    double s_ytd = stock.S_YTD;
    double s_order_cnt = stock.S_ORDER_CNT;
    stock.S_YTD = s_ytd + ol_quantity;
    stock.S_ORDER_CNT = s_order_cnt + 1;
    if (remote) {
      double s_remote_cnt = stock.S_REMOTE_CNT;
      s_remote_cnt++;
      stock.S_REMOTE_CNT = s_remote_cnt;
    }

    /*====================================================+
      EXEC SQL UPDATE stock SET s_quantity = :s_quantity
      WHERE s_i_id = :ol_i_id
      AND s_w_id = :ol_supply_w_id;
      stock->S_QUANTITY = quantity;
      +====================================================*/
    uint64_t quantity;
    if (s_quantity > ol_quantity + 10) {
      quantity = s_quantity - ol_quantity;
    } else {
      quantity = s_quantity - ol_quantity + 91;
    }
    stock.S_QUANTITY = (double) quantity;
    // TODO : tanabe will write below
    // update(token, Storage::STOCK, stock_key, stock, alignof(TPCC::Stock));

#ifdef DBx1000
    uint64_t stock_key = stockKey(ol_i_id, ol_supply_w_id);
    INDEX * stock_index = _wl->i_stock;
    itemid_t * stock_item;
    index_read(stock_index, stock_key, wh_to_part(ol_supply_w_id), stock_item);
    assert(item != NULL);
    row_t * r_stock = ((row_t *)stock_item->location);
    row_t * r_stock_local = get_row(r_stock, WR);
    if (r_stock_local == NULL) {
      return finish(Abort);
    }

    // XXX s_dist_xx are not retrieved.
    UInt64 s_quantity;
    int64_t s_remote_cnt;
    s_quantity = *(int64_t *)r_stock_local->get_value(S_QUANTITY);

#if !TPCC_SMALL
    int64_t s_ytd;
    int64_t s_order_cnt;
    r_stock_local->get_value(S_YTD, s_ytd);
    r_stock_local->set_value(S_YTD, s_ytd + ol_quantity);
    r_stock_local->get_value(S_ORDER_CNT, s_order_cnt);
    r_stock_local->set_value(S_ORDER_CNT, s_order_cnt + 1);
    s_data = r_stock_local->get_value(S_DATA);
#endif

    if (remote) {
      s_remote_cnt = *(int64_t*)r_stock_local->get_value(S_REMOTE_CNT);
      s_remote_cnt ++;
      r_stock_local->set_value(S_REMOTE_CNT, &s_remote_cnt);
    }
    uint64_t quantity;
    if (s_quantity > ol_quantity + 10) {
      quantity = s_quantity - ol_quantity;
    } else {
      quantity = s_quantity - ol_quantity + 91;
    }
    r_stock_local->set_value(S_QUANTITY, &quantity);

#endif /* DBx1000 */

    /*====================================================+
      EXEC SQL INSERT INTO order_line(ol_o_id, ol_d_id, ol_w_id, ol_number,	ol_i_id, ol_supply_w_id,
      ol_quantity, ol_amount, ol_dist_info)
      VALUES(:o_id, :d_id, :w_id, :ol_number,
      :ol_i_id, :ol_supply_w_id,
      :ol_quantity, :ol_amount, :ol_dist_info);
      +====================================================*/
    w_tax = wh->W_TAX;
    d_tax = dist.D_TAX;
    //amt[ol_number-1]=ol_amount;
    //total += ol_amount;
    double ol_amount = ol_quantity * i_price * (1.0 + w_tax + d_tax) * (1.0 - c_discount);

    TPCC::OrderLine orderline{};
    orderline.OL_O_ID = o_id;
    orderline.OL_D_ID = d_id;
    orderline.OL_W_ID = w_id;
    orderline.OL_NUMBER = ol_number;
    orderline.OL_I_ID = ol_i_id;
    orderline.OL_SUPPLY_W_ID = ol_supply_w_id;
    orderline.OL_QUANTITY = ol_quantity;
    orderline.OL_AMOUNT = ol_amount;
    strcpy(orderline.OL_DIST_INFO, "OL_DIST_INFO"); /* This is not implemented in DBx1000 */

#if !TPCC_SMALL
    // distinfo?
#endif
    // The number of keys is enough? It is different from DBx1000
    std::string ol_key = TPCC::OrderLine::CreateKey(orderline.OL_W_ID, orderline.OL_D_ID, orderline.OL_O_ID, orderline.OL_NUMBER);
    /**
     * TODO : check. Is it ok either insert operation successes or not.
     */
    stat = insert(token, Storage::ORDERLINE, ol_key, {(char *) &orderline, sizeof(TPCC::OrderLine)},
                  alignof(TPCC::OrderLine));
#ifdef DBx1000
    row_t * r_ol;
    uint64_t row_id;
    _wl->t_orderline->get_new_row(r_ol, 0, row_id);
    r_ol->set_value(OL_O_ID, &o_id);
    r_ol->set_value(OL_D_ID, &d_id);
    r_ol->set_value(OL_W_ID, &w_id);
    r_ol->set_value(OL_NUMBER, &ol_number);
    r_ol->set_value(OL_I_ID, &ol_i_id);

#if !TPCC_SMALL
    int w_tax=1, d_tax=1;
    int64_t ol_amount = ol_quantity * i_price * (1 + w_tax + d_tax) * (1 - c_discount);
    r_ol->set_value(OL_SUPPLY_W_ID, &ol_supply_w_id);
    r_ol->set_value(OL_QUANTITY, &ol_quantity);
    r_ol->set_value(OL_AMOUNT, &ol_amount);
#endif
    insert_row(r_ol, _wl->t_orderline);
  }
  assert( rc == RCOK );

  //return finish(rc);
#endif // DBx1000
  } // end of ol loop
  if (commit(token) == Status::OK) {
    return true;
  }
  abort(token);
  return false;
}

} // namespace TPCC
