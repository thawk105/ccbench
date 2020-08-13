#include "river.hh"
#include "interface.h"
#include "masstree_beta_wrapper.h"

using namespace ccbench;

bool init_table_warehouse(size_t nwh){
  Token token{};
	init();
  enter(token);
  //insert(token, Storage::WAREHOUSE, a, b);
	
	std::time_t now = std::time(nullptr);
	//CREATE Warehouses by single thread.
	for (size_t w = 0; w < nwh; w++) {
		TPCC::Warehouse *wh = (TPCC::Warehouse *)malloc(sizeof(TPCC::Warehouse));	if (!wh) ERR;
		wh->W_ID = w;
		wh->W_TAX = 1.5;
		wh->W_YTD = 1000;
		std::string key = wh->CreateKey(w);
		std::cout << wh->W_ID << std::endl;
		Status stat;		
		stat = insert(token, Storage::WAREHOUSE, key, {(char *)wh, sizeof(TPCC::Warehouse)});
		if (stat != Status::OK) ERR;
	}
	commit(token);

	for (size_t w = 0; w < nwh; w++) {
		TPCC::Warehouse wh;
		std::string key = wh.CreateKey(w);
		Tuple *ret_tuple_ptr;
		Status stat;

		stat = search_key(token, Storage::WAREHOUSE, key, &ret_tuple_ptr);
		if (stat != Status::OK) ERR;
		//wh = (TPCC::Warehouse &)ret_tuple_ptr;
		memcpy(&wh, ret_tuple_ptr, sizeof(TPCC::Warehouse));
		//std::cout << ((TPCC::Warehouse *) ret_tuple_ptr)->W_ID << std::endl;
		std::cout << wh.W_ID << std::endl;
	}
	commit(token);
	
	leave(token);
	fin();
}

void
makedb_tpcc()
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
	makedb_tpcc();
	const int nwh = 10;
	init_table_warehouse(nwh);
	
	return 0;
}
