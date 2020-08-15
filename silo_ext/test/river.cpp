#include "river.hh"
#include "interface.h"
#include "masstree_beta_wrapper.h"

using namespace ccbench;

void init_table_warehouse(size_t nwh){
  Token token{};
  enter(token);
  //insert(token, Storage::WAREHOUSE, a, b);
	
	{
		//CREATE Warehouses by single thread.
		for (size_t w = 0; w < nwh; w++) {
			TPCC::Warehouse wh{};
			wh.W_ID = w;
			wh.W_TAX = 1.5;
			wh.W_YTD = 1000'000'000;
			insert(token, Storage::WAREHOUSE, wh.CreateKey(w), (char *)&wh);
		}
	}
	commit(token);
	leave(token);
}

void
makedb_tpcc()
{
  Token token{};
  std::string a{"a"};
  std::string b{"b"};
	
  enter(token);
  insert(token, Storage::CUSTOMER, a, b);
  Tuple *ret_tuple_ptr;
  commit(token);
	
  delete_record(token, Storage::CUSTOMER, a);
	commit(token);
  search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr);
  commit(token);
  leave(token);
}

int
//tpcc_txn_man::run_new_order(tpcc_query * query)
main()
{
  init();
	makedb_tpcc();
	const int nwh = 224;
	init_table_warehouse(nwh);
	fin();
	return 0;
}
