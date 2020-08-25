#include "interface.h"
#include "masstree_beta_wrapper.h"
#include "tpcc/tpcc_query.hpp"

using namespace ccbench;

namespace TPCC {

bool
run_new_order(TPCC::query::NewOrder *query, Token &token) {
  //itemid_t * item;
  //INDEX * index;

  bool remote = query->remote;
  uint16_t w_id = query->w_id;
  uint8_t d_id = query->d_id;
  uint32_t c_id = query->c_id;
  uint8_t ol_cnt = query->ol_cnt;

  /*=======================================================================+
    EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
    INTO :c_discount, :c_last, :c_credit, :w_tax
    FROM customer, warehouse
    WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
    +========================================================================*/
  TPCC::Warehouse *wh;
  SimpleKey<8> wh_key;
  TPCC::Warehouse::CreateKey(w_id, wh_key.ptr());
  // index = _wl->i_warehouse;
  // item = index_read(index, key, wh_to_part(w_id));
  Tuple *ret_tuple_ptr;
  Status stat;
  stat = search_key(token, Storage::WAREHOUSE, wh_key.view(), &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  wh = (TPCC::Warehouse *) ret_tuple_ptr->get_val().data();

  [[maybe_unused]] double w_tax = wh->W_TAX;
  //uint64_t key = custKey(c_id, d_id, w_id);
  TPCC::Customer *cust;
  SimpleKey<8> cust_key;
  TPCC::Customer::CreateKey(w_id, d_id, c_id, cust_key.ptr());
  stat = search_key(token, Storage::CUSTOMER, cust_key.view(), &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  cust = reinterpret_cast<TPCC::Customer *>(const_cast<char *>(ret_tuple_ptr->get_val().data()));
  double c_discount = cust->C_DISCOUNT;
  [[maybe_unused]] char *c_last = cust->C_LAST;
  [[maybe_unused]] char *c_credit = cust->C_CREDIT;

  /*==================================================+
    EXEC SQL SELECT d_next_o_id, d_tax
    INTO :d_next_o_id, :d_tax
    FROM district WHERE d_id = :d_id AND d_w_id = :w_id;

    EXEC SQL UPDATE district SET d_next_o_id = :d_next_o_id + 1
    WHERE d_id = :d_id AND d_w_id = :w_id ;
    +===================================================*/
  SimpleKey<8> dist_key;
  TPCC::District::CreateKey(w_id, d_id, dist_key.ptr());
  stat = search_key(token, Storage::DISTRICT, dist_key.view(), &ret_tuple_ptr);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  HeapObject dist_obj;
  dist_obj.allocate<TPCC::District>();
  TPCC::District& dist = dist_obj.ref();
  memcpy(&dist, ret_tuple_ptr->get_val().data(), sizeof(dist));

  [[maybe_unused]] double d_tax = dist.D_TAX;
  std::uint32_t o_id = dist.D_NEXT_O_ID;
  o_id++;
  dist.D_NEXT_O_ID = o_id;
  // o_id = dist.D_NEXT_O_ID; /* no need to execute */
  stat = update(token, Storage::DISTRICT, Tuple(dist_key.view(), std::move(dist_obj)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  /*=========================================+
    EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
    VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
    +=======================================*/
  HeapObject order_obj;
  order_obj.allocate<TPCC::Order>();
  TPCC::Order& order = order_obj.ref();

  order.O_ID = o_id;
  order.O_D_ID = d_id;
  order.O_W_ID = w_id;
  order.O_C_ID = c_id;
  order.O_ENTRY_D = ccbench::epoch::get_lightweight_timestamp();
  order.O_OL_CNT = ol_cnt;
  order.O_ALL_LOCAL = (remote ? 0 : 1);
  SimpleKey<8> order_key;
  TPCC::Order::CreateKey(order.O_W_ID, order.O_D_ID, order.O_ID, order_key.ptr());
  stat = insert(token, Storage::ORDER, Tuple(order_key.view(), std::move(order_obj)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  /*=======================================================+
    EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
    VALUES (:o_id, :d_id, :w_id);
    +=======================================================*/

  HeapObject no_obj;
  no_obj.allocate<TPCC::NewOrder>();
  TPCC::NewOrder& neworder = no_obj.ref();
  neworder.NO_O_ID = o_id;
  neworder.NO_D_ID = d_id;
  neworder.NO_W_ID = w_id;
  SimpleKey<8> no_key;
  TPCC::NewOrder::CreateKey(neworder.NO_W_ID, neworder.NO_D_ID, neworder.NO_O_ID, no_key.ptr());
  stat = insert(token, Storage::NEWORDER, Tuple(no_key.view(), std::move(no_obj)));
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }

  for (std::uint32_t ol_number = 0; ol_number < ol_cnt; ++ol_number) {
    /*===========================================+
      EXEC SQL SELECT i_price, i_name , i_data
      INTO :i_price, :i_name, :i_data
      FROM item
      WHERE i_id = :ol_i_id;
      +===========================================*/

    std::uint64_t ol_i_id = query->items[ol_number].ol_i_id;
    std::uint64_t ol_supply_w_id = query->items[ol_number].ol_supply_w_id;
    std::uint64_t ol_quantity = query->items[ol_number].ol_quantity;

    /*
     * ### Unnecessary? ###
     * price[ol_number-1] = i_price;
     * strncpy(iname[ol_number-1],i_name,24);
     */

    TPCC::Item *item;
    SimpleKey<8> item_key;
    TPCC::Item::CreateKey(ol_i_id, item_key.ptr());
    stat = search_key(token, Storage::ITEM, item_key.view(), &ret_tuple_ptr);
    if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }
    item = (TPCC::Item *) ret_tuple_ptr->get_val().data();
    double i_price = item->I_PRICE;
    [[maybe_unused]] char *i_name = item->I_NAME;
    [[maybe_unused]] char *i_data = item->I_DATA;

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

    SimpleKey<8> stock_key;
    TPCC::Stock::CreateKey(ol_supply_w_id, ol_i_id, stock_key.ptr());
    stat = search_key(token, Storage::STOCK, stock_key.view(), &ret_tuple_ptr);
    if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }

    HeapObject s_obj;
    s_obj.allocate<TPCC::Stock>();
    TPCC::Stock& stock = s_obj.ref();
    const TPCC::Stock& stock_old =
        *reinterpret_cast<const TPCC::Stock*>(ret_tuple_ptr->get_val().data());
    memcpy(&stock, &stock_old, sizeof(stock));
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
    stat = update(token, Storage::STOCK, Tuple(stock_key.view(), std::move(s_obj)));
    if (stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }

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

    HeapObject ol_obj;
    ol_obj.allocate<TPCC::OrderLine>();
    TPCC::OrderLine& orderline = ol_obj.ref();
    orderline.OL_O_ID = o_id;
    orderline.OL_D_ID = d_id;
    orderline.OL_W_ID = w_id;
    orderline.OL_NUMBER = ol_number;
    orderline.OL_I_ID = ol_i_id;
    orderline.OL_SUPPLY_W_ID = ol_supply_w_id;
    orderline.OL_QUANTITY = ol_quantity;
    orderline.OL_AMOUNT = ol_amount;
#if 0
    strcpy(orderline.OL_DIST_INFO, "OL_DIST_INFO"); /* This is not implemented in DBx1000 */
#else
    auto pick_sdist = [&]() -> const char* {
        switch (d_id) {
        case 1: return stock_old.S_DIST_01;
        case 2: return stock_old.S_DIST_02;
        case 3: return stock_old.S_DIST_03;
        case 4: return stock_old.S_DIST_04;
        case 5: return stock_old.S_DIST_05;
        case 6: return stock_old.S_DIST_06;
        case 7: return stock_old.S_DIST_07;
        case 8: return stock_old.S_DIST_08;
        case 9: return stock_old.S_DIST_09;
        case 10: return stock_old.S_DIST_10;
        default: return nullptr; // BUG
        }
    };
    copy_cstr(orderline.OL_DIST_INFO, pick_sdist(), sizeof(orderline.OL_DIST_INFO));
#endif

    // The number of keys is enough? It is different from DBx1000
    SimpleKey<8> ol_key;
    TPCC::OrderLine::CreateKey(orderline.OL_W_ID, orderline.OL_D_ID, orderline.OL_O_ID,
                               orderline.OL_NUMBER, ol_key.ptr());
    stat = insert(token, Storage::ORDERLINE, Tuple(ol_key.view(), std::move(ol_obj)));
    if (stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }

  } // end of ol loop
  if (commit(token) == Status::OK) {
    return true;
  }
  abort(token);
  return false;
}

} // namespace TPCC
