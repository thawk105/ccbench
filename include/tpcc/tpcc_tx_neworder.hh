#pragma once

#include "../tuple_body.hh"

#include "tpcc_query.hh"
#include "tpcc_tables.hh"
#include "tpcc_util.hh"

/**
 * =======================================================================+
 * EXEC SQL SELECT c_discount, c_last, c_credit, w_tax
 * INTO :c_discount, :c_last, :c_credit, :w_tax
 * FROM customer, warehouse
 * WHERE w_id = :w_id AND c_w_id = w_id AND c_d_id = :d_id AND c_id = :c_id;
 * +========================================================================
 */
template <typename TxExecutor, typename TxStatus>
bool get_warehouse(TxExecutor& tx, uint16_t w_id, const Warehouse*& ware) {
  SimpleKey<8> w_key;
  Warehouse::CreateKey(w_id, w_key.ptr());
  TupleBody *body;
  Status stat = tx.read(Storage::Warehouse, w_key.view(), &body);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  ware = &body->get_value().cast_to<Warehouse>();
  return true;
}


template <typename TxExecutor, typename TxStatus>
bool get_customer(TxExecutor& tx, uint32_t c_id, uint8_t d_id, uint16_t w_id, const Customer*& cust) {
  SimpleKey<8> c_key;
  Customer::CreateKey(w_id, d_id, c_id, c_key.ptr());
  TupleBody *body;
  Status stat = tx.read(Storage::Customer, c_key.view(), &body);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  cust = &body->get_value().cast_to<Customer>();
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
template <typename TxExecutor, typename TxStatus>
bool get_and_update_district(TxExecutor& tx, uint8_t d_id, uint16_t w_id, const District*& dist) {
  SimpleKey<8> d_key;
  District::CreateKey(w_id, d_id, d_key.ptr());
  TupleBody *body;
  Status stat = tx.read(Storage::District, d_key.view(), &body);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  HeapObject d_obj;
  d_obj.allocate<District>();
  District& new_dist = d_obj.ref();
  const District& old_dist = body->get_value().cast_to<District>();
  memcpy(&new_dist, &old_dist, sizeof(new_dist));
  new_dist.D_NEXT_O_ID++;
  stat = tx.write(Storage::District, d_key.view(), TupleBody(d_key.view(), std::move(d_obj)));
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  dist = &new_dist;
  return true;
}


/**
 * =========================================+
 * EXEC SQL INSERT INTO ORDERS (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local)
 * VALUES (:o_id, :d_id, :w_id, :c_id, :datetime, :o_ol_cnt, :o_all_local);
 * +=======================================
 */
template <typename TxExecutor, typename TxStatus>
bool insert_order(TxExecutor& tx, uint32_t o_id, uint8_t d_id, uint16_t w_id, uint32_t c_id,
                  uint8_t ol_cnt, bool remote, const Order*& ord) {
  HeapObject o_obj;
  o_obj.allocate<Order>();
  Order& new_ord = o_obj.ref();
  new_ord.O_ID = o_id;
  new_ord.O_D_ID = d_id;
  new_ord.O_W_ID = w_id;
  new_ord.O_C_ID = c_id;
  new_ord.O_ENTRY_D = get_lightweight_timestamp();
  new_ord.O_OL_CNT = ol_cnt;
  new_ord.O_ALL_LOCAL = (remote ? 0 : 1);
  HeapObject key_obj;
  key_obj.allocate<SimpleKey<8>>();
  SimpleKey<8>& o_key = key_obj.ref();
  Order::CreateKey(new_ord.O_W_ID, new_ord.O_D_ID, new_ord.O_ID, o_key.ptr());
  Status stat = tx.insert(Storage::Order, o_key.view(), TupleBody(o_key.view(), std::move(o_obj)));
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat == Status::WARN_ALREADY_EXISTS || tx.status_ == TransactionStatus::aborted) {
    dump(tx.thid_, "insert order failed");
    return false;
  }
  SimpleKey<16> o_sec_key;
  Order::CreateSecondaryKey(w_id, d_id, c_id, o_id, o_sec_key.ptr());
  stat = tx.insert(Storage::OrderSecondary, o_sec_key.view(), TupleBody(o_sec_key.view(), std::move(key_obj)));
  if (stat == Status::WARN_ALREADY_EXISTS || tx.status_ == TransactionStatus::aborted) {
    dump(tx.thid_, "insert order-secondary failed");
    return false;
  }
  ord = &new_ord;
  return true;
}


/**
 * =======================================================+
 * EXEC SQL INSERT INTO NEW_ORDER (no_o_id, no_d_id, no_w_id)
 * VALUES (:o_id, :d_id, :w_id);
 * +=======================================================
 */
template <typename TxExecutor, typename TxStatus>
bool insert_neworder(TxExecutor& tx, uint32_t o_id, uint8_t d_id, uint16_t w_id) {
  HeapObject no_obj;
  no_obj.allocate<NewOrder>();
  NewOrder& new_no = no_obj.ref();
  new_no.NO_O_ID = o_id;
  new_no.NO_D_ID = d_id;
  new_no.NO_W_ID = w_id;
  SimpleKey<8> no_key;
  NewOrder::CreateKey(new_no.NO_W_ID, new_no.NO_D_ID, new_no.NO_O_ID, no_key.ptr());
  Status stat = tx.insert(Storage::NewOrder, no_key.view(), TupleBody(no_key.view(), std::move(no_obj)));
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat == Status::WARN_ALREADY_EXISTS) {
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
template <typename TxExecutor, typename TxStatus>
bool get_item(TxExecutor& tx, uint32_t ol_i_id, const Item*& item) {
  SimpleKey<8> i_key;
  Item::CreateKey(ol_i_id, i_key.ptr());
  TupleBody *body;
  Status stat = tx.read(Storage::Item, i_key.view(), &body);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  item = &body->get_value().cast_to<Item>();
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
template <typename TxExecutor, typename TxStatus>
bool get_and_update_stock(TxExecutor& tx, uint16_t ol_supply_w_id,
                          uint32_t ol_i_id, uint8_t ol_quantity,
                          bool remote, const Stock*& sto) {
    SimpleKey<8> s_key;
    Stock::CreateKey(ol_supply_w_id, ol_i_id, s_key.ptr());
    TupleBody *body;
    Status stat = tx.read(Storage::Stock, s_key.view(), &body);
    if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
    if (stat != Status::OK) {
      return false;
    }
    const Stock& old_sto = body->get_value().cast_to<Stock>();

    HeapObject s_obj;
    s_obj.allocate<Stock>();
    Stock& new_sto = s_obj.ref();
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

    stat = tx.write(Storage::Stock, s_key.view(), TupleBody(s_key.view(), std::move(s_obj)));
    if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
    if (stat != Status::OK) {
      return false;
    }
    sto = &new_sto;
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
template <typename TxExecutor, typename TxStatus>
bool insert_orderline(
  TxExecutor& tx, uint32_t o_id, uint8_t d_id, uint16_t w_id,
  uint8_t ol_num, uint32_t ol_i_id, uint16_t ol_supply_w_id,
  uint8_t ol_quantity, double ol_amount, const Stock* sto) {
  HeapObject ol_obj;
  ol_obj.allocate<OrderLine>();
  OrderLine& new_ol = ol_obj.ref();
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
  OrderLine::CreateKey(new_ol.OL_W_ID, new_ol.OL_D_ID, new_ol.OL_O_ID,
                       new_ol.OL_NUMBER, ol_key.ptr());
  Status stat = tx.insert(Storage::OrderLine, ol_key.view(), TupleBody(ol_key.view(), std::move(ol_obj)));
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat == Status::WARN_ALREADY_EXISTS) {
    return false;
  }
  return true;
}

template <typename TxExecutor, typename TxStatus>
bool run_new_order(TxExecutor& tx, TPCCQuery::NewOrder *query) {
  bool remote = query->remote;
  uint16_t w_id = query->w_id;
  uint8_t d_id = query->d_id;
  uint32_t c_id = query->c_id;
  uint8_t ol_cnt = query->ol_cnt;

  const Warehouse *ware;
  if (!get_warehouse<TxExecutor,TxStatus>(tx, w_id, ware)) return false;

  const Customer *cust;
  if (!get_customer<TxExecutor,TxStatus>(tx, c_id, d_id, w_id, cust)) return false;

  const District *dist;
  if (!get_and_update_district<TxExecutor,TxStatus>(tx, d_id, w_id, dist)) return false;

  uint32_t o_id = dist->D_NEXT_O_ID;
  [[maybe_unused]] const Order *ord;

  if (!insert_order<TxExecutor,TxStatus>(tx, o_id, d_id, w_id, c_id, ol_cnt, remote, ord)) return false;
  if (!insert_neworder<TxExecutor,TxStatus>(tx, o_id, d_id, w_id)) return false;

  for (std::uint32_t ol_num = 0; ol_num < ol_cnt; ++ol_num) {
    uint32_t ol_i_id = query->items[ol_num].ol_i_id;
    uint16_t ol_supply_w_id = query->items[ol_num].ol_supply_w_id;
    uint8_t ol_quantity = query->items[ol_num].ol_quantity;

    const Item *item;
    if (!get_item<TxExecutor,TxStatus>(tx, ol_i_id, item)) return false;

    const Stock *sto;
    if (!get_and_update_stock<TxExecutor,TxStatus>(
         tx, ol_supply_w_id, ol_i_id, ol_quantity, remote, sto)) return false;

    double i_price = item->I_PRICE;
    double w_tax = ware->W_TAX;
    double d_tax = dist->D_TAX;
    double c_discount = cust->C_DISCOUNT;
    double ol_amount = ol_quantity * i_price * (1.0 + w_tax + d_tax) * (1.0 - c_discount);

    if (!insert_orderline<TxExecutor,TxStatus>(
          tx, o_id, d_id, w_id, ol_num, ol_i_id,
          ol_supply_w_id, ol_quantity, ol_amount, sto)) return false;
  } // end of ol loop

  return true;
}
