/*
TPC-C
http://www.tpc.org/tpc_documents_current_versions/pdf/tpc-c_v5.11.0.pdf

4.3.3 Table Population Requirements

*/
#pragma once

#include "interface.h"
#include "./index/masstree_beta/include/masstree_beta_wrapper.h"
#include "tpcc_tables.hpp"
#include "common.hh"
#include "../include/random.hh"

#include <algorithm>
#include <cassert>
#include <thread>
#include <vector>

using namespace ccbench;

namespace TPCC::Initializer {
void db_insert(const Storage st, const std::string_view key, const std::string_view val, std::size_t val_align) {
  auto *record_ptr = new Record{key, val, val_align};
  record_ptr->get_tidw().set_absent(false);
  record_ptr->get_tidw().set_lock(false);
  if (Status::OK != kohler_masstree::insert_record(st, key, record_ptr)) {
    std::cout << __FILE__ << " : " << __LINE__ << " : " << "fatal error. unique key restriction." << std::endl;
    std::cout << "st : " << static_cast<int>(st) << ", key : " << key << ", val : " << val << std::endl;
    std::abort();
  }
}

std::string random_string(const std::uint64_t minLen, const std::uint64_t maxLen, Xoroshiro128Plus &rnd) {
  static const char alphanum[] =
          "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  int len = (rnd.next() % (maxLen - minLen + 1)) + minLen;
  std::string s(len, 'a');
  for (int i = 0; i < len; i++) {
    size_t rn = rnd.next();
    size_t idx = rn % (sizeof(alphanum)-1);
    s[i] = alphanum[idx];
  }
  return s;
}
    
std::string MakeNumberString(const std::uint64_t minLen, const std::uint64_t maxLen, Xoroshiro128Plus &rnd){
    static const char Numbers[] = "0123456789";
    int len = (rnd.next() % (maxLen - minLen + 1)) + minLen;
    std::string s(len, '1');
    for(int i=0;i<len;i++){
        size_t rn = rnd.next();
        size_t idx = rn % (sizeof(Numbers)-1);
        s[i] = Numbers[idx];
    }
    return s;
}

template<typename T>
T random_value(const T &minv, const T &maxv) {
  static thread_local std::mt19937 generator; // NOLINT
  if constexpr(std::is_integral<T>{}) {
    std::uniform_int_distribution<T> distribution(minv, maxv);
    return distribution(generator);
  } else {
    std::uniform_real_distribution<T> distribution(minv, maxv);
    return distribution(generator);
  }
}

std::string gen_zipcode(Xoroshiro128Plus &rnd) {
  std::string s(9, 'a');
  for (int i = 0; i < 9; i++) {
    if (i > 3)s[i] = '1';
    else s[i] = '0' + (rnd.next() % 10);
  }
  return s;
}

//TPC-C Reference 2.1.6 for detail specification.
/*
A is a constant chosen according to the size of range [x..y].
C is a run-time constant randomly chosen within [0..A]
*/
inline unsigned int NURand(int A, const int x, const int y) {
  const int C = random_value(0, A);
  assert(x <= y);
  return (((random_value(0, A) | random_value(x, y)) + C) % (y - x + 1)) + x;
}

static std::string createC_LAST(const std::size_t rndval) {
  const char *LAST_NAMES[10] = {"BAR", "OUGHT", "ABLE", "PRI", "PRES", "ESE", "ANTI", "CALLY", "ATION", "EING"};
  std::string s;
  s.reserve(16);
  for (int i = 2; i >= 0; i--) {
    const size_t digit = (rndval / (int) (std::pow(10, i)) % 10);
    s += LAST_NAMES[digit];
  }
  return s;
}


//CREATE Item
void load_item() {
  struct S {
    static void work(std::size_t start, std::size_t end) {
      Xoroshiro128Plus rnd{};
      rnd.init();
      for (size_t i = start; i <= end; ++i) {
        TPCC::Item ite{};
        ite.I_ID = i;
        ite.I_IM_ID = random_value(1, 10000);
        strcpy(ite.I_NAME, random_string(14, 24, rnd).c_str());
        ite.I_PRICE = random_value(1.00, 100.00);
        size_t dataLen=random_value(26,50);
        strcpy(ite.I_DATA, random_string(dataLen, dataLen, rnd).c_str());
        if(random_value<int>(1,100)<=10){
            std::uint64_t pos = random_value(0,(int)(dataLen-8));
            ite.I_DATA[pos]='O';
            ite.I_DATA[pos+1]='R';
            ite.I_DATA[pos+2]='I';
            ite.I_DATA[pos+3]='G';
            ite.I_DATA[pos+4]='I';
            ite.I_DATA[pos+5]='N';
            ite.I_DATA[pos+6]='A';
            ite.I_DATA[pos+7]='L';
       }        
#ifdef DEBUG
        if(i<3)std::cout<<"I_ID:"<<ite.I_ID<<"\tI_IM_ID:"<<ite.I_IM_ID<<"\tI_NAME:"<<ite.I_NAME<<"\tI_PRICE:"<<ite.I_PRICE<<"\tI_DATA:"<<ite.I_DATA<<std::endl;
#endif
        std::string key{ite.createKey()};
        db_insert(Storage::ITEM, key, {reinterpret_cast<char *>(&ite), sizeof(ite)}, alignof(TPCC::Item));
      }
    }
  };

  std::vector<std::thread> thv;
  /**
   * precondition : para_num > 3
   */
  constexpr std::size_t para_num{10};
  thv.emplace_back(S::work, 1, MAX_ITEMS / para_num);
  for (std::size_t i = 1; i < para_num - 1; ++i) {
    thv.emplace_back(S::work, (MAX_ITEMS / para_num) * i + 1, (MAX_ITEMS / para_num) * (i + 1));
  }
  thv.emplace_back(S::work, (MAX_ITEMS / para_num) * (para_num - 1) + 1, MAX_ITEMS);

  for (auto &&th : thv) {
    th.join();
  }
}

//CREATE Warehouses
void load_warehouse(const std::size_t w) {
  Xoroshiro128Plus rnd{};
  rnd.init();
  TPCC::Warehouse ware{};
  ware.W_ID = w;
  strcpy(ware.W_NAME, random_string(6, 10, rnd).c_str());
  strcpy(ware.W_STREET_1, random_string(10, 20, rnd).c_str());
  strcpy(ware.W_STREET_2, random_string(10, 20, rnd).c_str());
  strcpy(ware.W_CITY, random_string(10, 20, rnd).c_str());
  strcpy(ware.W_STATE, random_string(2, 2, rnd).c_str());
  strcpy(ware.W_ZIP, gen_zipcode(rnd).c_str());
  ware.W_TAX = random_value(0.0, 0.20);
  ware.W_YTD = 300000;

#ifdef DEBUG
  std::cout<<"W_ID:"<<ware.W_ID<<"\tW_NAME:"<<ware.W_NAME<<"\tW_STREET_1:"<<ware.W_STREET_1<<"\tW_CITY:"<<ware.W_CITY<<"\tW_STATE:"<<ware.W_STATE<<"\tW_ZIP:"<<ware.W_ZIP<<"\tW_TAX:"<<ware.W_TAX<<"\tW_YTD:"<<ware.W_YTD<<std::endl;
#endif
  std::string key{ware.createKey()};
  db_insert(Storage::WAREHOUSE, key, {reinterpret_cast<char *>(&ware), sizeof(ware)}, alignof(TPCC::Warehouse));
}

//CREATE Stock
void load_stock(const std::size_t w) {
  struct S {
    static void work(const std::size_t start, const std::size_t end, const std::size_t w) {
      Xoroshiro128Plus rnd{};
      rnd.init();
      for (std::size_t s = start; s <= end; ++s) {
        TPCC::Stock st{};
        st.S_I_ID = s;
        st.S_W_ID = w;
        st.S_QUANTITY = random_value(10.0, 100.0);
        strcpy(st.S_DIST_01, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_02, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_03, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_04, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_05, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_06, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_07, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_08, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_09, random_string(24, 24, rnd).c_str());
        strcpy(st.S_DIST_10, random_string(24, 24, rnd).c_str());
        st.S_YTD = 0;
        st.S_ORDER_CNT = 0;
        st.S_REMOTE_CNT = 0;
        size_t dataLen = random_value(26,50);
        strcpy(st.S_DATA, random_string(dataLen, dataLen, rnd).c_str());
        if(random_value<int>(1,100)<=10){
            std::uint64_t pos = random_value(0,(int)(dataLen-8));
            st.S_DATA[pos]='O';
            st.S_DATA[pos+1]='R';
            st.S_DATA[pos+2]='I';
            st.S_DATA[pos+3]='G';
            st.S_DATA[pos+4]='I';
            st.S_DATA[pos+5]='N';
            st.S_DATA[pos+6]='A';
            st.S_DATA[pos+7]='L';
        }
        std::string key{st.createKey()};
        db_insert(Storage::STOCK, key, {reinterpret_cast<char *>(&st), sizeof(st)}, alignof(TPCC::Stock));
      }
    }
  };
  const std::size_t stock_num{w * MAX_ITEMS};
  constexpr std::size_t stock_num_per_thread{500};
  const std::size_t para_num{stock_num / stock_num_per_thread};
  std::vector<std::thread> thv;
  thv.emplace_back(S::work, 1, stock_num_per_thread, w);
  for (std::size_t i = 1; i < para_num - 1; ++i) {
    thv.emplace_back(S::work, i * stock_num_per_thread + 1, (i + 1) * stock_num_per_thread, w);
  }
  thv.emplace_back(S::work, (para_num - 1) * stock_num_per_thread + 1, stock_num, w);

  for (auto &&th : thv) {
    th.join();
  }
}

//CREATE History
void load_history(const std::size_t w, const std::size_t d, const std::size_t c, const std::string &&key) {
  Xoroshiro128Plus rnd{};
  rnd.init();
  std::time_t now = std::time(nullptr);
  TPCC::History history{};
  history.H_C_ID = c;
  history.H_C_D_ID = history.H_D_ID = d;
  history.H_C_W_ID = history.H_W_ID = w;
  history.H_DATE = now;
  history.H_AMOUNT = 10.00;
  strcpy(history.H_DATA, random_string(12, 24, rnd).c_str());

  db_insert(Storage::HISTORY, key, {reinterpret_cast<char *>(&history), sizeof(history)}, alignof(TPCC::History));
}

//CREATE Orderline
void load_orderline(const std::size_t w, const std::size_t d, const std::size_t c, const std::size_t ol) {
  Xoroshiro128Plus rnd{};
  rnd.init();
  std::time_t now = std::time(nullptr);
  TPCC::OrderLine order_line{};
  order_line.OL_O_ID = c;
  order_line.OL_D_ID = d;
  order_line.OL_W_ID = w;
  order_line.OL_NUMBER = ol;
  order_line.OL_I_ID = random_value(1, 100000);
  order_line.OL_SUPPLY_W_ID = w;
  if (order_line.OL_O_ID < 2101) {
    order_line.OL_DELIVERY_D = now;
  } else {
    order_line.OL_DELIVERY_D = 0;
  }
  order_line.OL_QUANTITY = 5;
  if (order_line.OL_O_ID < 2101) {
    order_line.OL_AMOUNT = 0.00;
  } else {
    order_line.OL_AMOUNT = random_value(0.01, 99999.99);
  }
  order_line.OL_AMOUNT = 0.0;
  strcpy(order_line.OL_DIST_INFO, random_string(24, 24, rnd).c_str());

  std::string key = order_line.createKey();
  db_insert(Storage::ORDERLINE, key, {reinterpret_cast<char *>(&order_line), sizeof(order_line)},
            alignof(TPCC::OrderLine));

}

//CREATE Order
void load_order(const std::size_t w, const std::size_t d, const std::size_t c) {
  Xoroshiro128Plus rnd{};
  rnd.init();
  std::time_t now = std::time(nullptr);
  TPCC::Order order{};
  order.O_ID = c;
  order.O_C_ID = c; //selected sequentially from a random permutation of [1 .. 3,000]
  order.O_D_ID = d;
  order.O_W_ID = w;
  order.O_ENTRY_D = now;
  if (order.O_ID < 2101) {
    order.O_CARRIER_ID = random_value(1, 10);
  } else {
    order.O_CARRIER_ID = 0;
  }
  order.O_OL_CNT = random_value(5, 15);
  order.O_ALL_LOCAL = 1;

  std::string key = order.createKey();
  db_insert(Storage::ORDER, key, {reinterpret_cast<char *>(&order), sizeof(order)}, alignof(TPCC::Order));

  //O_OL_CNT orderlines per order.
  for (size_t ol = 1; ol < order.O_OL_CNT + 1; ol++) {
    load_orderline(w, d, c, ol);
  }

  //CREATE NewOrder 900 rows
  if (2100 < c) {
    TPCC::NewOrder new_order{};
    new_order.NO_O_ID = c;
    new_order.NO_D_ID = d;
    new_order.NO_W_ID = w;

    key = new_order.createKey();
    db_insert(Storage::NEWORDER, key, {reinterpret_cast<char *>(&new_order), sizeof(new_order)},
              alignof(TPCC::NewOrder));
  }
}

//CREATE Customer
void load_customer(const std::size_t d, const std::size_t w, TPCC::HistoryKeyGenerator &hkg) {
  struct S {
    static void
    work(const std::size_t start, const std::size_t end, TPCC::HistoryKeyGenerator &hkg,
         const std::size_t d, const std::size_t w) {
      Xoroshiro128Plus rnd{};
      rnd.init();
      for (size_t c = start; c <= end; ++c) {
        std::time_t now = std::time(nullptr);
        TPCC::Customer customer{};
        customer.C_ID = c;
        customer.C_D_ID = d;
        customer.C_W_ID = w;
        if (c < 1000) {
          //for the first 1,000 customers, and generating a non -uniform random number using the function NURand(255,0,999)
          strcpy(customer.C_LAST, createC_LAST(NURand(255, 0, 999)).c_str());
#ifdef DEBUG
          std::cout<<"C_LAST:"<<customer.C_LAST<<std::endl;
#endif
        } else {
          strcpy(customer.C_LAST, createC_LAST(random_value<int>(0, 999)).c_str());
        }
        strcpy(customer.C_MIDDLE, "OE");
        strcpy(customer.C_FIRST, random_string(8, 16, rnd).c_str());
        strcpy(customer.C_STREET_1, random_string(10, 20, rnd).c_str());
        strcpy(customer.C_STREET_2, random_string(10, 20, rnd).c_str());
        strcpy(customer.C_CITY, random_string(10, 20, rnd).c_str());
        strcpy(customer.C_STATE, random_string(2, 2, rnd).c_str());
        strcpy(customer.C_ZIP, gen_zipcode(rnd).c_str());
        //C_PHONE
        strcpy(customer.C_PHONE, MakeNumberString(16,16,rnd).c_str());
#ifdef DEBUG
        if(c==start&& w==1&& d==2)std::cout<<"C_PHONE:"<<customer.C_PHONE<<std::endl;
#endif        
        customer.C_SINCE = now;
        //90% GC 10% BC
        if(random_value(0,99)>=90){
            strcpy(customer.C_CREDIT, "BC");
        }else{
            strcpy(customer.C_CREDIT, "GC");
        }
        customer.C_CREDIT_LIM = 50000.00;
        customer.C_DISCOUNT = random_value(0.0000, 0.50000);
        customer.C_BALANCE = -10.00;
        customer.C_YTD_PAYMENT = 10.00;
        customer.C_PAYMENT_CNT = 1;
        customer.C_DELIVERY_CNT = 0;
        strcpy(customer.C_DATA, random_string(300, 500, rnd).c_str());

        std::string key = customer.createKey();
        db_insert(Storage::CUSTOMER, key, {reinterpret_cast<char *>(&customer), sizeof(customer)},
                  alignof(TPCC::Customer));

        void *rec_ptr = kohler_masstree::find_record(Storage::CUSTOMER, key);
        key = customer.createSecondaryKey();
        std::vector<void *> *ctn_ptr;
        void *ret_ptr = kohler_masstree::find_record(Storage::SECONDARY, key);
        if (ret_ptr != nullptr) {
          memcpy(&ctn_ptr,
                 reinterpret_cast<void *>(const_cast<char *>(reinterpret_cast<Record *>(ret_ptr)->get_tuple().get_val().data())),
                 8);
          ctn_ptr->emplace_back(rec_ptr);
        } else {
          ctn_ptr = new std::vector<void *>;
          ctn_ptr->emplace_back(rec_ptr);
          db_insert(Storage::SECONDARY, key, {reinterpret_cast<char *>(&ctn_ptr), sizeof(ctn_ptr)},
                    alignof(std::vector<void *> *));
        }

        struct S {
          static bool comp(const void *lh, const void *rh) {
            std::string_view lh_key_view = static_cast<const Record *>(lh)->get_tuple().get_key();
            std::string_view rh_key_view = static_cast<const Record *>(rh)->get_tuple().get_key();
            int ret = memcmp(lh_key_view.data(), rh_key_view.data(),
                             lh_key_view.size() < rh_key_view.size() ? lh_key_view.size() : rh_key_view.size());
            if (ret < 0) {
              return true;
            } else if (ret == 0) {
              return lh_key_view.size() < rh_key_view.size();
            } else {
              return false;
            }
          }
        };

        std::sort(ctn_ptr->begin(), ctn_ptr->end(), S::comp);
        //1 histories per customer.
        std::string his_key = std::to_string(hkg.get());
        load_history(w, d, c, static_cast<const std::string &&>(his_key));
        //1 order per customer.
        load_order(w, d, c);
      }
    }
  };
  S::work(1, CUST_PER_DIST, std::ref(hkg), d, w);
  #if 0
  constexpr std::size_t cust_num_per_th{500};
  constexpr std::size_t para_num{CUST_PER_DIST / cust_num_per_th};
  std::vector<std::thread> thv;
  thv.emplace_back(S::work, 1, cust_num_per_th, std::ref(hkg), d, w);
  for (std::size_t i = 1; i < para_num - 1; ++i) {
    thv.emplace_back(S::work, i * cust_num_per_th + 1, (i + 1) * cust_num_per_th, std::ref(hkg), d, w);
  }
  thv.emplace_back(S::work, (para_num - 1) * cust_num_per_th + 1, CUST_PER_DIST, std::ref(hkg), d, w);

  for (auto &&th : thv) {
    th.join();
  }
  #endif
}

void load_district(const std::size_t w) {
  struct S {
    static void work(const std::size_t d, const std::size_t w, TPCC::HistoryKeyGenerator &hkg) {
      Xoroshiro128Plus rnd{};
      rnd.init();
      TPCC::District district{};
      district.D_ID = d;
      district.D_W_ID = w;
      strcpy(district.D_NAME, random_string(6, 10, rnd).c_str());
      strcpy(district.D_STREET_1, random_string(10, 20, rnd).c_str());
      strcpy(district.D_STREET_2, random_string(10, 20, rnd).c_str());
      strcpy(district.D_CITY, random_string(10, 20, rnd).c_str());
      strcpy(district.D_STATE, random_string(2, 2, rnd).c_str());
      strcpy(district.D_ZIP, gen_zipcode(rnd).c_str());
      district.D_TAX = random_value(0.0000, 2.0000);
      district.D_YTD = 30000.00;
      district.D_NEXT_O_ID = 3001;

#ifdef DEBUG
      std::cout<<"D_ID:"<<district.D_ID<<std::endl;
#endif
      
      std::string key{district.createKey()};
      db_insert(Storage::DISTRICT, key, {reinterpret_cast<char *>(&district), sizeof(district)},
                alignof(TPCC::District));

      // CREATE Customer History Order Orderline. 3000 customers per a district.
      load_customer(d, w, hkg);
    }
  };
  TPCC::HistoryKeyGenerator hkg{};
  hkg.init(w, true);
  
  std::vector<std::thread> thv;
  for (size_t d = 1; d <= DIST_PER_WARE; ++d) {
    thv.emplace_back(S::work, d, w, std::ref(hkg));
  }

  for (auto &&th : thv) {
    th.join();
  }
  
}

void load() {
  //ID 1-origin

  std::vector<std::thread> thv;
  std::cout << "[start] load." << std::endl;
  thv.emplace_back(load_item);
  for (std::size_t w = 1; w <= FLAGS_num_wh; ++w) {
    thv.emplace_back(load_warehouse, w);
    //100,000 stocks per warehouse
    thv.emplace_back(load_stock, w);
    //10 districts per warehouse
    thv.emplace_back(load_district, w);
  }

  for (auto &&th : thv) {
    th.join();
  }
  std::cout << "[end] load." << std::endl;
}

}//namespace TPCC initializer
