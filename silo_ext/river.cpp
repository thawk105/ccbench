#include "river.hh"
#include "interface.h"
#include "masstree_beta_wrapper.h"

using namespace ccbench;

bool load(size_t warehouse){
  Token token{};
	init();
  enter(token);
  //insert(token, Storage::WAREHOUSE, a, b);
	
	std::time_t now = std::time(nullptr);
	{
		//CREATE Warehouses by single thread.
		for (size_t w = 0; w < warehouse; w++) {
			TPCC::Warehouse wh;
			wh.W_ID = w;
			wh.W_TAX = 1.5;
			wh.W_YTD = 1000'000'000;
			insert(token, Storage::WAREHOUSE, wh.createKey(), wh);
		}
	}
}

void
makeDBforTPCC()
{
  Token token{};
  std::string a{"a"};
  std::string b{"b"};

	init();
  enter(token);
  insert(token, Storage::CUSTOMER, a, b);
  Tuple *ret_tuple_ptr;
  commit(token);
	
  delete_record(token, Storage::CUSTOMER, a);
	commit(token);
  search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr);
  commit(token);
  leave(token);
	fin();
}

int
//tpcc_txn_man::run_new_order(tpcc_query * query)
main(void)
{
	makeDBforTPCC();

	return 0;
}
