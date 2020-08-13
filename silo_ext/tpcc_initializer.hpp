/*
TPC-C
http://www.tpc.org/tpc_documents_current_versions/pdf/tpc-c_v5.11.0.pdf

1.3 Table Layouts

*/
#pragma once

#include "./include/interface.h"
#include "./index/masstree_beta/include/masstree_beta_wrapper.h"
#include "tpcc_tables.hpp"
#include "../include/random.hh"
#include <cassert>


using namespace ccbench;

namespace TPCC::Initializer {
void db_insert(Storage st, std::string_view key, std::string_view val) {
  auto *record_ptr = new Record{key, val};
  if (Status::OK != kohler_masstree::insert_record(st, key, record_ptr)) {
    std::cout << __FILE__ << " : " << __LINE__ << " : " << "fatal error. unique key restriction." << std::endl;
    std::cout << "st : " << static_cast<int>(st) << ", key : " << key << ", val : " << val << std::endl;
    std::abort();
  }
}
char* random_string(int minLen,int maxLen,Xoroshiro128Plus &rnd){
    static const char alphanum[]=
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int len=(rnd.next()%(maxLen-minLen+1))+minLen;
    //std::cout<<"len="<<len<<std::endl;
    char* s=(char*)malloc(sizeof(char)*(len+1));
    for(int i=0;i<len;i++){
        size_t rn = rnd.next();
        int idx=rn%(sizeof(alphanum));
        assert(idx<len);
        s[i]=alphanum[idx];
    }
    s[len]='\0';
    return s;
}
template<typename T>
T random_value(const T& minv,const T& maxv){
    static thread_local std::mt19937 generator;
    if constexpr(std::is_integral<T>{}){
      std::uniform_int_distribution<T>distribution(minv,maxv);
      return distribution(generator);
    }else{
      std::uniform_real_distribution<T> distribution(minv,maxv);
      return distribution(generator);
    }
}
char* gen_zipcode(Xoroshiro128Plus &rnd){
    char* s=(char*)malloc(sizeof(char)*(9));
    for(int i=0;i<9;i++){
        if(i>3)s[i]='1';
        else s[i]='0'+(rnd.next()%10);
    }
    return s;
}

bool load(size_t warehouse) {
  std::time_t now = std::time(nullptr);
  Xoroshiro128Plus rnd;

  {
    //CREATE Item
    for(size_t i=0;i<100000;i++){
      TPCC::Item item;
      item.I_ID = i;
      item.I_IM_ID = random_value(1,10000);
      strcpy(item.I_NAME,random_string(14,24,rnd));
      item.I_PRICE = random_value(1.00,100.00);
      strcpy(item.I_DATA,random_string(26,50,rnd));
      //TODO ORIGINAL

      #ifdef DEBUG
      if(i<3)std::cout<<"I_ID:"<<item.I_ID<<"\tI_IM_ID:"<<item.I_IM_ID<<"\tI_NAME:"<<item.I_NAME<<"\tI_PRICE:"<<item.I_PRICE<<"\tI_DATA:"<<item.I_DATA<<std::endl;
      #endif
    }

  }

  {
    //CREATE Warehouses by single thread.
    for (size_t w = 0; w < warehouse; w++) {
      TPCC::Warehouse ware;
      ware.W_ID = w;      
      strcpy(ware.W_NAME,random_string(6,10,rnd));
      strcpy(ware.W_STREET_1,random_string(10,20,rnd));
      strcpy(ware.W_STREET_2,random_string(10,20,rnd));
      strcpy(ware.W_CITY,random_string(10,20,rnd));
      strcpy(ware.W_STATE,random_string(2,2,rnd));
      strcpy(ware.W_ZIP,gen_zipcode(rnd));
      ware.W_TAX = random_value(0.0,0.20);
      ware.W_YTD = 300000;

      #ifdef DEBUG
      std::cout<<"W_ID:"<<ware.W_ID<<"\tW_NAME:"<<ware.W_NAME<<"\tW_STREET_1:"<<ware.W_STREET_1<<"\tW_CITY:"<<ware.W_CITY<<"\tW_STATE:"<<ware.W_STATE<<"\tW_ZIP:"<<ware.W_ZIP<<"\tW_TAX:"<<ware.W_TAX<<"\tW_YTD:"<<ware.W_YTD<<std::endl;
      #endif

      std::string key{std::move(ware.createKey())};
      db_insert(Storage::WAREHOUSE, key, {reinterpret_cast<char *>(&ware), sizeof(ware)});
    }
  }
  {
    
    for (size_t w = 0; w < warehouse; w++) {
      //100,000 stocks per warehouse
      for(size_t s=0;s<100000;s++){
          TPCC::Stock stock;
          stock.S_I_ID = s;
          stock.S_W_ID = w;
          stock.S_QUANTITY = random_value(10.0,100.0);
          strcpy(stock.S_DIST_01,random_string(24,24,rnd));
          strcpy(stock.S_DIST_02,random_string(24,24,rnd));
          strcpy(stock.S_DIST_03,random_string(24,24,rnd));
          strcpy(stock.S_DIST_04,random_string(24,24,rnd));
          strcpy(stock.S_DIST_05,random_string(24,24,rnd));
          strcpy(stock.S_DIST_06,random_string(24,24,rnd));
          strcpy(stock.S_DIST_07,random_string(24,24,rnd));
          strcpy(stock.S_DIST_08,random_string(24,24,rnd));
          strcpy(stock.S_DIST_09,random_string(24,24,rnd));
          strcpy(stock.S_DIST_10,random_string(24,24,rnd));
          stock.S_YTD=0;
          stock.S_ORDER_CNT=0;
          stock.S_REMOTE_CNT=0;
          strcpy(stock.S_DATA,random_string(26,50,rnd));
          //TODO ORIGINAL
          
      }
        
      //10 districts per warehouse
      for (size_t d = 0; d < 10; d++) {
        TPCC::District district;
        district.D_ID = d;
        district.D_W_ID = w;
        district.D_NEXT_O_ID = 0;
        district.D_TAX = 1.5;
        district.D_YTD = 1000'000'000;

        std::string key{std::move(district.createKey())};
        db_insert(Storage::DISTRICT, key, {reinterpret_cast<char *>(&district), sizeof(district)});

        // CREATE Customer. 3000 customers per a district.
        for (size_t c = 0; c < 3000; c++) {
          TPCC::Customer customer;
          customer.C_ID = c;
          customer.C_D_ID = d;
          customer.C_W_ID = w;
          strcpy(customer.C_LAST, "SUZUKI");
          customer.C_SINCE = now;
          strcpy(customer.C_CREDIT, "GC");
          customer.C_DISCOUNT = 0.1;
          customer.C_BALANCE = -10.00;
          customer.C_YTD_PAYMENT = 10.00;
          customer.C_PAYMENT_CNT = 1;
          customer.C_PAYMENT_CNT = 1;
          customer.C_DELIVERY_CNT = 0;

          key = std::move(customer.createKey());
          db_insert(Storage::CUSTOMER, key, {reinterpret_cast<char *>(&customer), sizeof(customer)});

          //CREATE History. 1 histories per customer.
          TPCC::History history;
          history.H_C_ID = c;
          history.H_C_D_ID = history.H_D_ID = d;
          history.H_C_W_ID = w;
          history.H_DATE = now;

          // TODO : History class doesn't has createKey function.
          // std::string key{std::move(history.createKey())};
          // db_insert(Storage::HISTORY, key, {reinterpret_cast<char *>(&history), sizeof(history)});

          //CREATE Order. 1 order per customer.
          TPCC::Order order;
          order.O_ID = c;
          order.O_C_ID = c;
          order.O_D_ID = d;
          order.O_W_ID = w;
          order.O_ENTRY_D = now;

          key = std::move(order.createKey());
          db_insert(Storage::ORDER, key, {reinterpret_cast<char *>(&order), sizeof(order)});

          //CREATE OrderLine. O_OL_CNT orderlines per order.
          for (size_t ol = 0; ol < order.O_OL_CNT; ol++) {
            TPCC::OrderLine order_line;
            order_line.OL_O_ID = order.O_ID;
            order_line.OL_D_ID = d;
            order_line.OL_NUMBER = ol;
            order_line.OL_I_ID = 1;
            order_line.OL_SUPPLY_W_ID = w;
            order_line.OL_DELIVERY_D = now;
            order_line.OL_AMOUNT = 0.0;

            key = std::move(order_line.createKey());
            db_insert(Storage::ORDERLINE, key, {reinterpret_cast<char *>(&order_line), sizeof(order_line)});
          }

          //CREATE NewOrder 900 rows.
          //900 rows in the NEW-ORDER table corresponding to the last
          //3000-2100=900
          if (2100 < order.O_ID) {
            TPCC::NewOrder new_order;
            new_order.NO_O_ID = order.O_ID;
            new_order.NO_D_ID = order.O_D_ID;
            new_order.NO_W_ID = order.O_W_ID;

            key = std::move(new_order.createKey());
            db_insert(Storage::NEWORDER, key, {reinterpret_cast<char *>(&new_order), sizeof(new_order)});
          }
        }
      }
    }
  }
}

}//namespace TPCC initializer



