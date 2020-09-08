#include "interface.h"
#include "masstree_beta_wrapper.h"
#include "tpcc/tpcc_query.hpp"
#include "common.hh"

using namespace ccbench;

namespace TPCC {

namespace {


/**
 * =======================================================================+
 * EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
 * INTO :c_discount, :c_last, :c_credit, :w_tax
 * FROM customer, warehouse
 * WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
 * +========================================================================
 */
bool get_warehouse(Token& token, uint16_t w_id, const TPCC::Warehouse*& ware)
{
  SimpleKey<8> w_key;
  TPCC::Warehouse::CreateKey(w_id, w_key.ptr());
  Tuple *tuple;
  Status stat = search_key(token, Storage::WAREHOUSE, w_key.view(), &tuple);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  ware = &tuple->get_value().cast_to<TPCC::Warehouse>();
  return true;
}


bool get_customer(
  Token& token, uint32_t c_id, uint8_t d_id, uint16_t w_id,
  const TPCC::Customer*& cust)
{
  SimpleKey<8> c_key;
  TPCC::Customer::CreateKey(w_id, d_id, c_id, c_key.ptr());
  Tuple *tuple;
  Status stat = search_key(token, Storage::CUSTOMER, c_key.view(), &tuple);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  cust = &tuple->get_value().cast_to<TPCC::Customer>();
  return true;
}


/**
 * ==================================================+
 * EXEC SQL SELECT d_next_o_id, d_tax
 * INTO :d_next_o_id, :d_tax
 * FROM district WHERE d_id = :d_id AND d_w_id = :w_id;
 *
 * EXEC SQL UPDATE district SET d_next_o_id = :d_next_o_id + 1
 * WHERE d_id = :d_id AND d_w_id = :w_id ;
 * +===================================================
 */
bool get_and_update_district(
    Token& token, uint8_t d_id, uint16_t w_id,
    const TPCC::District*& dist)
{
  SimpleKey<8> d_key;
  TPCC::District::CreateKey(w_id, d_id, d_key.ptr());
  Tuple *tuple;
  Status stat = search_key(token, Storage::DISTRICT, d_key.view(), &tuple);
  if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  HeapObject d_obj;
  d_obj.allocate<TPCC::District>();
  TPCC::District& new_dist = d_obj.ref();
  const TPCC::District& old_dist = tuple->get_value().cast_to<TPCC::District>();
  memcpy(&new_dist, &old_dist, sizeof(new_dist));
  new_dist.D_NEXT_O_ID++;
  stat = update(token, Storage::DISTRICT, Tuple(d_key.view(), std::move(d_obj)), &tuple);
  if (stat == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  dist = &tuple->get_value().cast_to<TPCC::District>();
  return true;
}


/**
 * =========================================+
 * EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
 * VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
 * +=======================================
 */
bool insert_order(
  Token& token, uint32_t o_id, uint8_t d_id, uint16_t w_id, uint32_t c_id,
  uint8_t ol_cnt, bool remote,
  const TPCC::Order*& ord)
{
  HeapObject o_obj;
  o_obj.allocate<TPCC::Order>();
  TPCC::Order& new_ord = o_obj.ref();
  new_ord.O_ID = o_id;
  new_ord.O_D_ID = d_id;
  new_ord.O_W_ID = w_id;
  new_ord.O_C_ID = c_id;
  new_ord.O_ENTRY_D = ccbench::epoch::get_lightweight_timestamp();
  new_ord.O_OL_CNT = ol_cnt;
  new_ord.O_ALL_LOCAL = (remote ? 0 : 1);
  SimpleKey<8> o_key;
  TPCC::Order::CreateKey(new_ord.O_W_ID, new_ord.O_D_ID, new_ord.O_ID, o_key.ptr());
  Tuple *tuple;
  Status sta = insert(token, Storage::ORDER, Tuple(o_key.view(), std::move(o_obj)), &tuple);
  if (sta == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  ord = &tuple->get_value().cast_to<TPCC::Order>();
  return true;
}


/**
 * =======================================================+
 * EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
 * VALUES (:o_id, :d_id, :w_id);
 * +=======================================================
 */
bool insert_neworder(Token& token, uint32_t o_id, uint8_t d_id, uint16_t w_id)
{
  HeapObject no_obj;
  no_obj.allocate<TPCC::NewOrder>();
  TPCC::NewOrder& new_no = no_obj.ref();
  new_no.NO_O_ID = o_id;
  new_no.NO_D_ID = d_id;
  new_no.NO_W_ID = w_id;
  SimpleKey<8> no_key;
  TPCC::NewOrder::CreateKey(new_no.NO_W_ID, new_no.NO_D_ID, new_no.NO_O_ID, no_key.ptr());
  Status sta = insert(token, Storage::NEWORDER, Tuple(no_key.view(), std::move(no_obj)));
  if (sta == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  return true;
}


/**
 * ===========================================+
 * EXEC SQL SELECT i_price, i_name , i_data
 * INTO :i_price, :i_name, :i_data
 * FROM item
 * WHERE i_id = :ol_i_id;
 * +===========================================
 */
bool get_item(Token& token, uint32_t ol_i_id, const TPCC::Item*& item)
{
  SimpleKey<8> i_key;
  TPCC::Item::CreateKey(ol_i_id, i_key.ptr());
  Tuple *tuple;
  Status sta = search_key(token, Storage::ITEM, i_key.view(), &tuple);
  if (sta == Status::WARN_CONCURRENT_DELETE || sta == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  item = &tuple->get_value().cast_to<TPCC::Item>();
  return true;
}


/** ===================================================================+
 * EXEC SQL SELECT s_quantity, s_data,
 * s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05,
 * s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10
 * INTO :s_quantity, :s_data,
 * :s_dist_01, :s_dist_02, :s_dist_03, :s_dist_04, :s_dist_05,
 * :s_dist_06, :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10
 * FROM stock
 * WHERE s_i_id = :ol_i_id AND s_w_id = :ol_supply_w_id;
 *
 * EXEC SQL UPDATE stock SET s_quantity = :s_quantity
 * WHERE s_i_id = :ol_i_id
 * AND s_w_id = :ol_supply_w_id;
 * +===============================================
 */
bool get_and_update_stock(
  Token& token, uint16_t ol_supply_w_id, uint32_t ol_i_id, uint8_t ol_quantity, bool remote,
  const TPCC::Stock*& sto)
{
    SimpleKey<8> s_key;
    TPCC::Stock::CreateKey(ol_supply_w_id, ol_i_id, s_key.ptr());
    Tuple *tuple;
    Status stat = search_key(token, Storage::STOCK, s_key.view(), &tuple);
    if (stat == Status::WARN_CONCURRENT_DELETE || stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }
    const TPCC::Stock& old_sto = tuple->get_value().cast_to<TPCC::Stock>();

    HeapObject s_obj;
    s_obj.allocate<TPCC::Stock>();
    TPCC::Stock& new_sto = s_obj.ref();
    memcpy(&new_sto, &old_sto, sizeof(new_sto));

    new_sto.S_YTD = old_sto.S_YTD + ol_quantity;
    new_sto.S_ORDER_CNT = old_sto.S_ORDER_CNT + 1;
    if (remote) {
      new_sto.S_REMOTE_CNT = old_sto.S_REMOTE_CNT + 1;
    }

    int32_t s_quantity = old_sto.S_QUANTITY;
    int32_t quantity = s_quantity - ol_quantity;
    if (s_quantity <= ol_quantity + 10) quantity += 91;
    new_sto.S_QUANTITY = quantity;

    stat = update(token, Storage::STOCK, Tuple(s_key.view(), std::move(s_obj)), &tuple);
    if (stat == Status::WARN_NOT_FOUND) {
      abort(token);
      return false;
    }
    sto = &tuple->get_value().cast_to<TPCC::Stock>();
    return true;
}


/**
 * ====================================================+
 * EXEC SQL INSERT INTO order_line(ol_o_id, ol_d_id, ol_w_id, ol_num, ol_i_id, ol_supply_w_id,
 * ol_quantity, ol_amount, ol_dist_info)
 * VALUES(:o_id, :d_id, :w_id, :ol_number,
 * :ol_i_id, :ol_supply_w_id,
 * :ol_quantity, :ol_amount, :ol_dist_info);
 * +====================================================
 */
bool insert_orderline(
  Token& token, uint32_t o_id, uint8_t d_id, uint16_t w_id,
  uint8_t ol_num, uint32_t ol_i_id, uint16_t ol_supply_w_id,
  uint8_t ol_quantity, double ol_amount, const TPCC::Stock* sto)
{
  HeapObject ol_obj;
  ol_obj.allocate<TPCC::OrderLine>();
  TPCC::OrderLine& new_ol = ol_obj.ref();
  new_ol.OL_O_ID = o_id;
  new_ol.OL_D_ID = d_id;
  new_ol.OL_W_ID = w_id;
  new_ol.OL_NUMBER = ol_num;
  new_ol.OL_I_ID = ol_i_id;
  new_ol.OL_SUPPLY_W_ID = ol_supply_w_id;
  new_ol.OL_QUANTITY = ol_quantity;
  new_ol.OL_AMOUNT = ol_amount;
  auto pick_sdist = [&]() -> const char* {
    switch (d_id) {
    case 1: return sto->S_DIST_01;
    case 2: return sto->S_DIST_02;
    case 3: return sto->S_DIST_03;
    case 4: return sto->S_DIST_04;
    case 5: return sto->S_DIST_05;
    case 6: return sto->S_DIST_06;
    case 7: return sto->S_DIST_07;
    case 8: return sto->S_DIST_08;
    case 9: return sto->S_DIST_09;
    case 10: return sto->S_DIST_10;
    default: return nullptr; // BUG
    }
  };
  copy_cstr(new_ol.OL_DIST_INFO, pick_sdist(), sizeof(new_ol.OL_DIST_INFO));

  SimpleKey<8> ol_key;
  TPCC::OrderLine::CreateKey(new_ol.OL_W_ID, new_ol.OL_D_ID, new_ol.OL_O_ID,
                             new_ol.OL_NUMBER, ol_key.ptr());
  Status sta = insert(token, Storage::ORDERLINE, Tuple(ol_key.view(), std::move(ol_obj)));
  if (sta == Status::WARN_NOT_FOUND) {
    abort(token);
    return false;
  }
  return true;
}


} // unnamed namespace


bool run_new_order(TPCC::query::NewOrder *query, Token &token)
{
  bool remote = query->remote;
  uint16_t w_id = query->w_id;
  uint8_t d_id = query->d_id;
  uint32_t c_id = query->c_id;
  uint8_t ol_cnt = query->ol_cnt;

  const TPCC::Warehouse *ware;
  if (!get_warehouse(token, w_id, ware)) return false;

  const TPCC::Customer *cust;
  if (!get_customer(token, c_id, d_id, w_id, cust)) return false;

  const TPCC::District *dist;
  if (!get_and_update_district(token, d_id, w_id, dist)) return false;

  uint32_t o_id = dist->D_NEXT_O_ID;
  [[maybe_unused]] const TPCC::Order *ord;

  if (FLAGS_insert_exe) {
    if (!insert_order(token, o_id, d_id, w_id, c_id, ol_cnt, remote, ord)) return false;
    if (!insert_neworder(token, o_id, d_id, w_id)) return false;
  }

  for (std::uint32_t ol_num = 0; ol_num < ol_cnt; ++ol_num) {
    uint32_t ol_i_id = query->items[ol_num].ol_i_id;
    uint16_t ol_supply_w_id = query->items[ol_num].ol_supply_w_id;
    uint8_t ol_quantity = query->items[ol_num].ol_quantity;

    const TPCC::Item *item;
    if (!get_item(token, ol_i_id, item)) return false;

    const TPCC::Stock *sto;
    if (!get_and_update_stock(
          token, ol_supply_w_id, ol_i_id, ol_quantity, remote, sto)) return false;

    if (FLAGS_insert_exe) {
      double i_price = item->I_PRICE;
      double w_tax = ware->W_TAX;
      double d_tax = dist->D_TAX;
      double c_discount = cust->C_DISCOUNT;
      double ol_amount = ol_quantity * i_price * (1.0 + w_tax + d_tax) * (1.0 - c_discount);

      if (!insert_orderline(
            token, o_id, d_id, w_id, ol_num, ol_i_id,
            ol_supply_w_id, ol_quantity, ol_amount, sto)) return false;
    }
  } // end of ol loop
  if (commit(token) == Status::OK) {
    return true;
  }
  abort(token);
  return false;
}

} // namespace TPCC
