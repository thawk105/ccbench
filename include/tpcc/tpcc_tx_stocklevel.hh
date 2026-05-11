#pragma once

#include "../tuple_body.hh"

#include "tpcc_query.hh"
#include "tpcc_tables.hh"
#include "tpcc_util.hh"

template <typename TxExecutor, typename TxStatus>
bool get_district(TxExecutor& tx, uint8_t d_id, uint16_t w_id, const District*& dist) {
  SimpleKey<8> d_key;
  District::CreateKey(w_id, d_id, d_key.ptr());
  TupleBody *body;
  Status stat = tx.read(Storage::District, d_key.view(), &body);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  dist = &body->get_value().cast_to<District>();
  return true;
}

template <typename TxExecutor, typename TxStatus>
bool get_stock(TxExecutor& tx, uint16_t w_id, uint32_t i_id, const Stock*& stock) {
  SimpleKey<8> s_key;
  Stock::CreateKey(w_id, i_id, s_key.ptr());
  TupleBody *body;
  Status stat = tx.read(Storage::Stock, s_key.view(), &body);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (stat != Status::OK) {
    return false;
  }
  stock = &body->get_value().cast_to<Stock>();
  return true;
}

template <typename TxExecutor, typename TxStatus>
bool run_stock_level(TxExecutor& tx, TPCCQuery::StockLevel *query) {
  uint16_t w_id = query->w_id;
  uint8_t d_id = query->d_id;
  uint8_t threshold = query->threshold;

  const District* dist;
  if (!get_district<TxExecutor,TxStatus>(tx, d_id, w_id, dist)) return false;

  std::vector<TupleBody*> result;
  SimpleKey<8> low, up;
  OrderLine::CreateKey(w_id, d_id, dist->D_NEXT_O_ID - 20, 1, low.ptr());
  OrderLine::CreateKey(w_id, d_id, dist->D_NEXT_O_ID, 1, up.ptr());
  Status stat = tx.scan(Storage::OrderLine, low.view(), false, up.view(), true, result);
  if (FLAGS_tpcc_interactive_ms) sleepMs(FLAGS_tpcc_interactive_ms);
  if (tx.status_ == TransactionStatus::aborted) {
    return false;
  }

  // Collect item IDs first since read/write set vector might be extended
  // and the pointer will be obsolete even though this is not the best way.
  std::vector<uint32_t> ol_i_ids;
  for (auto& tuple : result) {
    const OrderLine& o = tuple->get_value().cast_to<OrderLine>();
    if (o.OL_I_ID != 1) { // if not unused item id
      ol_i_ids.emplace_back(o.OL_I_ID);
    }
  }

  uint32_t low_stock = 0;
  const Stock* stock;
  for (auto& ol_i_id : ol_i_ids) {
    if (!get_stock<TxExecutor,TxStatus>(tx, w_id, ol_i_id, stock)) return false;
    if (stock->S_QUANTITY < threshold) low_stock++;
  }

  return true;
}
