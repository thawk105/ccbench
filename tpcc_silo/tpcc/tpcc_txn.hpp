/**
 * @file tpcc_txn.hpp
 */

#pragma once

namespace TPCC {
bool run_new_order(query::NewOrder *query, Token &token);

bool run_payment(query::Payment *query, HistoryKeyGenerator *hkg, Token &token);
}
