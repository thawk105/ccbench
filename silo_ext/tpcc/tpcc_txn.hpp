namespace TPCC {
  bool run_new_order(query::NewOrder *query);
  bool run_payment(query::Payment *query, HistoryKeyGenerator *hkg);
}
