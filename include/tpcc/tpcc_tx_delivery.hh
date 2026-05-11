#pragma once

#include "../tuple_body.hh"

#include "tpcc_query.hh"
#include "tpcc_tables.hh"
#include "tpcc_util.hh"

// The row in the NEW-ORDER table with matching NO_W_ID (equals
// W_ID) and NO_D_ID (equals D_ID) and with the lowest NO_O_ID
// value is selected. This is the oldest undelivered order of that
// district. NO_O_ID, the order number, is retrieved.
// If no matching row is found, then the delivery of an order for
// this district is skipped.
// The condition in which no outstanding order is present at a
// given district must be handled by skipping the delivery of an
// order for that district only and resuming the delivery of an
// order from all remaining districts of the selected
// warehouse.
// If this condition occurs in more than 1%, or in more than one,
// whichever is greater, of the business transactions, it must be
// reported.
// The result file must be organized in such a way that the
// percentage of skipped deliveries and skipped districts can be
// determined.

// EXEC SQL DECLARE c_no CURSOR FOR
// SELECT no_o_id
// FROM new_order
// WHERE no_d_id = :d_id AND no_w_id = :w_id
// ORDER BY no_o_id ASC;
// EXEC SQL OPEN c_no;
// EXEC SQL WHENEVER NOT FOUND continue;
// EXEC SQL FETCH c_no INTO :no_o_id;
template <typename TxExecutor, typename TxStatus>
bool get_order_id(TxExecutor& tx, uint16_t w_id, uint8_t d_id, uint32_t &o_id) {
  std::vector<TupleBody*> result;
  SimpleKey<8> left_key, right_key;
  NewOrder::CreateKey(w_id, d_id, 1, left_key.ptr());
  NewOrder::CreateKey(w_id, d_id + 1, 1, right_key.ptr());
  Status status = tx.scan(Storage::NewOrder, left_key.view(), false, right_key.view(), true, result, 1);
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  TupleBody* body = *result.begin();
  o_id = body->get_value().cast_to<NewOrder>().NO_O_ID;
  return true;
}

// The selected row in the NEW-ORDER table is deleted.

// EXEC SQL DELETE FROM new_order WHERE CURRENT OF c_no;
// EXEC SQL CLOSE c_no;
template <typename TxExecutor, typename TxStatus>
bool delete_new_order(TxExecutor& tx, uint16_t w_id, uint8_t d_id, uint32_t o_id) {
  SimpleKey<8> no_key;
  NewOrder::CreateKey(w_id, d_id, o_id, no_key.ptr());
  Status status = tx.delete_record(Storage::NewOrder, no_key.view());
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  return true;
}

// The row in the ORDER table with matching O_W_ID (equals W_ ID),
// O_D_ID (equals D_ID), and O_ID (equals NO_O_ID) is selected,
// O_C_ID, the customer number, is retrieved, and O_CARRIER_ID is
// updated.

// EXEC SQL SELECT o_c_id INTO :c_id FROM orders
// WHERE o_id = :no_o_id AND o_d_id = :d_id AND
// o_w_id = :w_id;
// EXEC SQL UPDATE orders SET o_carrier_id = :o_carrier_id
// WHERE o_id = :no_o_id AND o_d_id = :d_id AND
// o_w_id = :w_id;
template <typename TxExecutor, typename TxStatus>
bool update_order_and_get_c_id(
    TxExecutor& tx, uint16_t w_id, uint8_t d_id, uint32_t o_id, uint8_t o_carrier_id, uint32_t &c_id) {
  SimpleKey<8> o_key;
  Order::CreateKey(w_id, d_id, o_id, o_key.ptr());
  TupleBody *body;
  Status status = tx.read(Storage::Order, o_key.view(), &body);
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  Order &ord = body->get_value().cast_to<Order>();
  c_id = ord.O_C_ID;

  HeapObject o_obj;
  o_obj.allocate<Order>();
  Order &new_ord = o_obj.ref();
  memcpy(&new_ord, &ord, sizeof(new_ord));

  new_ord.O_CARRIER_ID = o_carrier_id;

  status = tx.write(Storage::Order, o_key.view(), TupleBody(o_key.view(), std::move(o_obj)));
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  return true;
}

// All rows in the ORDER-LINE table with matching OL_W_ID (equals
// O_W_ID), OL_D_ID (equals O_D_ID), and OL_O_ID (equals O_ID) are
// selected.
// All OL_DELIVERY_D, the delivery dates, are updated to the
// current system time as returned by the operating system and the
// sum of all OL_AMOUNT is retrieved.

// EXEC SQL UPDATE order_line SET ol_delivery_d = :datetime
// WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id AND
// ol_w_id = :w_id;
// EXEC SQL SELECT SUM(ol_amount) INTO :ol_total
// FROM order_line
// WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id
// AND ol_w_id = :w_id;
template <typename TxExecutor, typename TxStatus>
bool update_order_line_and_get_ol_total(
    TxExecutor& tx, uint16_t w_id, uint8_t d_id, uint32_t o_id, uint64_t ol_delivery_d, double &ol_total)
{
  std::vector<TupleBody*> result;
  SimpleKey<8> left_key, right_key;
  OrderLine::CreateKey(w_id, d_id, o_id, 1, left_key.ptr());
  OrderLine::CreateKey(w_id, d_id, o_id + 1, 1, right_key.ptr());
  Status status = tx.scan(Storage::OrderLine, left_key.view(), false, right_key.view(), true, result);
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  ol_total = 0.0;
  for (auto& tuple : result)  {
    const std::string_view ol_key = tuple->get_key();
    const OrderLine &ol = tuple->get_value().cast_to<OrderLine>();

    HeapObject ol_obj;
    ol_obj.allocate<OrderLine>();
    OrderLine &new_ol = ol_obj.ref();
    memcpy(&new_ol, &ol, sizeof(new_ol));

    new_ol.OL_DELIVERY_D = ol_delivery_d;

    status = tx.write(Storage::OrderLine, ol_key, TupleBody(ol_key, std::move(ol_obj)));
    if (status != Status::OK) ERR;
    if (tx.status_ == TransactionStatus::aborted) {
      return false;
    }
    ol_total += ol.OL_AMOUNT;
  }
  return true;
}

// The row in the CUSTOMER table with matching C_W_ID (equals
// W_ID), C_D_ID (equals D_ID), and C_ID (equals O_C_ID) is
// selected and C_BALANCE is increased by the sum of all
// order-line amounts (OL_AMOUNT) previously retrieved.
// C_DELIVERY_CNT is incremented by 1.

// EXEC SQL UPDATE customer SET c_balance = c_balance + :ol_total
// WHERE c_id = :c_id AND c_d_id = :d_id AND
// c_w_id = :w_id;
template <typename TxExecutor, typename TxStatus>
bool update_customer_balance(
    TxExecutor& tx, uint16_t w_id, uint8_t d_id, uint32_t c_id, double ol_total)
{
  SimpleKey<8> c_key;
  Customer::CreateKey(w_id, d_id, c_id, c_key.ptr());
  TupleBody *body;
  Status status = tx.read(Storage::Customer, c_key.view(), &body);
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  Customer &cust = body->get_value().cast_to<Customer>();

  HeapObject c_obj;
  c_obj.allocate<Customer>();
  Customer &new_cust = c_obj.ref();
  memcpy(&new_cust, &cust, sizeof(new_cust));

  new_cust.C_BALANCE += ol_total;
  new_cust.C_DELIVERY_CNT += 1;

  status = tx.write(Storage::Customer, c_key.view(), TupleBody(c_key.view(), std::move(c_obj)));
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }
  return true;
}

template <typename TxExecutor, typename TxStatus>
bool run_delivery(TxExecutor &tx, TPCCQuery::Delivery *query) {
  uint16_t w_id = query->w_id;
  uint8_t o_carrier_id = query->o_carrier_id;
  uint64_t ol_delivery_d = query->ol_delivery_d;

  for (uint8_t d_id = 1; d_id <= DIST_PER_WARE; ++d_id) {
    uint32_t o_id;
    if (!get_order_id<TxExecutor,TxStatus>(tx, w_id, d_id, o_id))
      return false;

    if (!delete_new_order<TxExecutor,TxStatus>(tx, w_id, d_id, o_id))
      return false;

    uint32_t c_id;
    if (!update_order_and_get_c_id<TxExecutor,TxStatus>(tx, w_id, d_id, o_id, o_carrier_id, c_id))
      return false;

    double ol_total = 0.0;
    if (!update_order_line_and_get_ol_total<TxExecutor,TxStatus>(tx, w_id, d_id, o_id, ol_delivery_d, ol_total))
      return false;

    if (!update_customer_balance<TxExecutor,TxStatus>(tx, w_id, d_id, c_id, ol_total))
      return false;
  }

  return true;
}
