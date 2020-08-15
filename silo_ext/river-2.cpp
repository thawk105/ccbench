#include "river.hh"
#include "interface.h"
#include "masstree_beta_wrapper.h"

using namespace ccbench;

void init_table_warehouse(size_t nwh, Token token){
	std::time_t now = std::time(nullptr);
	//CREATE Warehouses by single thread.
	for (size_t w = 0; w < nwh; w++) {
		auto *wh = (TPCC::Warehouse *)malloc(sizeof(TPCC::Warehouse));	if (!wh) ERR;
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
		TPCC::Warehouse wh{};
		std::string key = TPCC::Warehouse::CreateKey(w);
		Tuple *ret_tuple_ptr;
		Status stat;

		stat = search_key(token, Storage::WAREHOUSE, key, &ret_tuple_ptr);
		if (stat != Status::OK) ERR;
		memcpy(&wh, ret_tuple_ptr->get_val().data(), sizeof(TPCC::Warehouse));
		std::cout << wh.W_ID << std::endl;
	}
	commit(token);
}

void
makedb_tpcc(Token token)
{
  std::string a{"a"};
  std::string b{"b"};
	
  insert(token, Storage::CUSTOMER, a, b);
  Tuple *ret_tuple_ptr;
  commit(token);
	
  delete_record(token, Storage::CUSTOMER, a);
	commit(token);

  search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr);
  commit(token);
}

int
//tpcc_txn_man::run_new_order(tpcc_query * query)
main()
{
  Token token{};

	init();
  enter(token);

	makedb_tpcc(token);
	const int nwh = 10;
	init_table_warehouse(nwh, token);
	leave(token);
	fin();

	return 0;
}
