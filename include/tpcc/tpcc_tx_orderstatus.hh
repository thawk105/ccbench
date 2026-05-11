#pragma once

#include "../tuple_body.hh"

#include "tpcc_query.hh"
#include "tpcc_tables.hh"
#include "tpcc_util.hh"

// The row in the ORDER table with matching O_W_ID (equals C_W_ID),
// O_D_ID (equals C_D_ID), O_C_ID (equals C_ID), and with the
// largest existing O_ID, is selected. This is the most recent order
// placed by that customer. O_ID, O_ENTRY_D, and O_CARRIER_ID are
// retrieved.
//
// EXEC SQL SELECT o_id, o_carrier_id, o_entry_d
//   INTO :o_id, :o_carrier_id, :entdate
//   FROM orders
//   ORDER BY o_id DESC;
template <typename TxExecutor, typename TxStatus, typename Tuple>
bool get_order_key_by_customer_id(TxExecutor& tx,
                                  uint16_t w_id, uint8_t d_id, uint32_t c_id,
                                  SimpleKey<8>& o_key) {
//  Storage storage = Storage::OrderSecondary;
//  char left_key_buf[16], right_key_buf[16];
//  std::string_view left_key = Order::CreateSecondaryKey(w_id, d_id, c_id, 1, &left_key_buf[0]);
//  std::string_view right_key = Order::CreateSecondaryKey(w_id, d_id, c_id+1, 1, &right_key_buf[0]);
  std::vector<TupleBody*> result;
  SimpleKey<16> left_key, right_key;
  Order::CreateSecondaryKey(w_id, d_id, c_id, 1, left_key.ptr());
  Order::CreateSecondaryKey(w_id, d_id, c_id+1, 1, right_key.ptr());
  Status status = tx.scan(Storage::OrderSecondary, left_key.view(), false, right_key.view(), true, result, 1);
  if (status != Status::OK || tx.status_ == TransactionStatus::aborted) {
    dump(tx.thid_, "cannot get order ID by scanning order-secondary with customer ID");
    return false;
  }
  if (result.size() != 1) ERR;
  TupleBody* body = *result.begin();
  o_key = body->get_value().cast_to<SimpleKey<8>>();
  return true;
}

// All rows in the ORDER-LINE table with matching OL_W_ID (equals
// O_W_ID), OL_D_ID (equals O_D_ID), and OL_O_ID (equals O_ID) are
// selected and the corresponding sets of OL_I_ID, OL_SUPPLY_W_ID,
// OL_QUANTITY, OL_AMOUNT, and OL_DELIVERY_D are retrieved.
//
// EXEC SQL DECLARE c_line CURSOR FOR
//   SELECT ol_i_id, ol_supply_w_id, ol_quantity,
//     ol_amount, ol_delivery_d
//   FROM order_line
//   WHERE ol_o_id=:o_id AND ol_d_id=:d_id AND ol_w_id=:w_id;
// EXEC SQL OPEN c_line;
// EXEC SQL WHENEVER NOT FOUND CONTINUE;
// i=0;
// while (sql_notfound(FALSE))
//   {
//     i++;
//     EXEC SQL FETCH c_line
//       INTO :ol_i_id[i], :ol_supply_w_id[i], :ol_quantity[i],
//       :ol_amount[i], :ol_delivery_d[i];
//   }
// EXEC SQL CLOSE c_line;
template <typename TxExecutor, typename TxStatus>
bool get_orderline(TxExecutor& tx, uint16_t w_id, uint8_t d_id, uint32_t o_id) {
  std::vector<TupleBody*> result;
  SimpleKey<8> left_key, right_key;
  OrderLine::CreateKey(w_id, d_id, o_id, 1, left_key.ptr());
  OrderLine::CreateKey(w_id, d_id, o_id+1, 1, right_key.ptr());
  Status status = tx.scan(
      Storage::OrderLine, left_key.view(), false, right_key.view(), true, result);
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  for (auto& tuple : result) {
    TupleBody* body;
    [[maybe_unused]] const OrderLine& ol = tuple->get_value().cast_to<OrderLine>();
  }
  return true;
}

bool get_customer_key_by_last_name(
  uint16_t w_id, uint8_t d_id, const char* c_last, SimpleKey<8>& c_key);

template <typename TxExecutor, typename TxStatus, typename Tuple>
bool run_order_status(TxExecutor& tx, TPCCQuery::OrderStatus *query) {
  uint16_t w_id = query->w_id;
  uint8_t d_id = query->d_id;
  uint32_t c_id = query->c_id;

  SimpleKey<8> key;
  if (query->by_last_name) {
    if (!get_customer_key_by_last_name<Tuple>(w_id, d_id, query->c_last, key))
      return false;
  } else {
    // search customers by c_id
    Customer::CreateKey(w_id, d_id, query->c_id, key.ptr());
    // EXEC SQL SELECT c_balance, c_first, c_middle, c_last
    //   INTO :c_balance, :c_first, :c_middle, :c_last
    //   FROM customer
    //   WHERE c_id=:c_id AND c_d_id=:d_id AND c_w_id=:w_id;
  }

  // get customer
  TupleBody *body;
  Status stat = tx.read(Storage::Customer, key.view(), &body);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  const Customer& cust = body->get_value().cast_to<Customer>();
  c_id = cust.C_ID;

  // search order by c_id
  if (!get_order_key_by_customer_id<TxExecutor,TxStatus,Tuple>(tx, w_id, d_id, c_id, key))
    return false;

  // get order
  stat = tx.read(Storage::Order, key.view(), &body);
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  const Order& o = body->get_value().cast_to<Order>();

  // scan orderline
  if (!get_orderline<TxExecutor,TxStatus>(tx, o.O_W_ID, o.O_D_ID, o.O_ID))
    return false;

  return true;
}
