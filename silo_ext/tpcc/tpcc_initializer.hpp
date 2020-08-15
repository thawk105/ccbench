/*
TPC-C
http://www.tpc.org/tpc_documents_current_versions/pdf/tpc-c_v5.11.0.pdf

4.3.3 Table Population Requirements

*/
#pragma once

#include "interface.h"
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
std::string random_string(int minLen,int maxLen,Xoroshiro128Plus &rnd){
    static const char alphanum[]=
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int len=(rnd.next()%(maxLen-minLen+1))+minLen;
    //std::cout<<"len="<<len<<std::endl;
    std::string s(len+1,'a');
    for(int i=0;i<len;i++){
        size_t rn = rnd.next();
        int idx=rn%(sizeof(alphanum));
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
std::string gen_zipcode(Xoroshiro128Plus &rnd){
    std::string s(10,'a');
    for(int i=0;i<9;i++){
        if(i>3)s[i]='1';
        else s[i]='0'+(rnd.next()%10);
    }
    s[9]='\0';
    return s;
}

//TPC-C Reference 2.1.6 for detail specification.
/*
A is a constant chosen according to the size of range [x..y].
C is a run-time constant randomly chosen within [0..A]
*/
inline int NURand(int A, const int x,const int y) {
  const int C = random_value(0,A);
  assert(x <= y);
  return ((( random_value(0,A) | random_value(x, y)) + C) % (y - x + 1)) + x;
}
    
static std::string createC_LAST(size_t rndval){
    const char* LAST_NAMES[10]={"BAR","OUGHT","ABLE","PRI","PRES","ESE","ANTI","CALLY","ATION","EING"};
    std::string s;
    s.reserve(16);
    for(int i=2;i>=0;i--){
        const size_t digit=(rndval/(int)(std::pow(10,i))%10);
        s+=LAST_NAMES[digit];
    }
    return s;
}
    
void load(size_t warehouse) {
  std::time_t now = std::time(nullptr);
  Xoroshiro128Plus rnd{};
  rnd.init();
  TPCC::HistoryKeyGenerator hkg{};
  hkg.init(255);
  
  {
    //CREATE Item
    for(size_t i=0;i<100000;i++){
      TPCC::Item item{};
      item.I_ID = i;
      item.I_IM_ID = random_value(1,10000);
      strcpy(item.I_NAME,random_string(14,24,rnd).c_str());
      item.I_PRICE = random_value(1.00,100.00);
      strcpy(item.I_DATA,random_string(26,50,rnd).c_str());
      //TODO ORIGINAL

      #ifdef DEBUG
      if(i<3)std::cout<<"I_ID:"<<item.I_ID<<"\tI_IM_ID:"<<item.I_IM_ID<<"\tI_NAME:"<<item.I_NAME<<"\tI_PRICE:"<<item.I_PRICE<<"\tI_DATA:"<<item.I_DATA<<std::endl;
      #endif
    }

  }

  {
    //CREATE Warehouses by single thread.
    for (size_t w = 0; w < warehouse; w++) {
      TPCC::Warehouse ware{};
      ware.W_ID = w;      
      strcpy(ware.W_NAME,random_string(6,10,rnd).c_str());
      strcpy(ware.W_STREET_1,random_string(10,20,rnd).c_str());
      strcpy(ware.W_STREET_2,random_string(10,20,rnd).c_str());
      strcpy(ware.W_CITY,random_string(10,20,rnd).c_str());
      strcpy(ware.W_STATE,random_string(2,2,rnd).c_str());
      strcpy(ware.W_ZIP,gen_zipcode(rnd).c_str());
      ware.W_TAX = random_value(0.0,0.20);
      ware.W_YTD = 300000;

      #ifdef DEBUG
      std::cout<<"W_ID:"<<ware.W_ID<<"\tW_NAME:"<<ware.W_NAME<<"\tW_STREET_1:"<<ware.W_STREET_1<<"\tW_CITY:"<<ware.W_CITY<<"\tW_STATE:"<<ware.W_STATE<<"\tW_ZIP:"<<ware.W_ZIP<<"\tW_TAX:"<<ware.W_TAX<<"\tW_YTD:"<<ware.W_YTD<<std::endl;
      #endif

      std::string key{ware.createKey()};
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
          strcpy(stock.S_DIST_01,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_02,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_03,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_04,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_05,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_06,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_07,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_08,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_09,random_string(24,24,rnd).c_str());
          strcpy(stock.S_DIST_10,random_string(24,24,rnd).c_str());
          stock.S_YTD=0;
          stock.S_ORDER_CNT=0;
          stock.S_REMOTE_CNT=0;
          strcpy(stock.S_DATA,random_string(26,50,rnd).c_str());
          //TODO ORIGINAL
          
      }
        
      //10 districts per warehouse
      for (size_t d = 0; d < 10; d++) {
        TPCC::District district{};
        district.D_ID = d;
        district.D_W_ID = w;
        strcpy(district.D_NAME,random_string(6,10,rnd).c_str());
        strcpy(district.D_STREET_1,random_string(10,20,rnd).c_str());
        strcpy(district.D_STREET_2,random_string(10,20,rnd).c_str());
        strcpy(district.D_CITY,random_string(10,20,rnd).c_str());
        strcpy(district.D_STATE,random_string(2,2,rnd).c_str());
        strcpy(district.D_ZIP,gen_zipcode(rnd).c_str());
        district.D_TAX = random_value(0.0000,2.0000);
        district.D_YTD = 30000.00;
        district.D_NEXT_O_ID = 3001;

        std::string key{district.createKey()};
        db_insert(Storage::DISTRICT, key, {reinterpret_cast<char *>(&district), sizeof(district)});
        
        // CREATE Customer. 3000 customers per a district.
        for (size_t c = 0; c < 3000; c++) {
          TPCC::Customer customer{};
          customer.C_ID = c;
          customer.C_D_ID = d;
          customer.C_W_ID = w;
          if(c<1000){
              //for the first 1,000 customers, and generating a non -uniform random number using the function NURand(255,0,999) 
              strcpy(customer.C_LAST,createC_LAST(NURand(255,0,999)).c_str());
              #ifdef DEBUG
              if(w==0&&d==0&&c==0)std::cout<<"C_LAST:"<<customer.C_LAST<<std::endl;
              #endif
          }
          else {
              strcpy(customer.C_LAST,createC_LAST(random_value<int>(0,999)).c_str());
          }
          strcpy(customer.C_MIDDLE,"OE");
          strcpy(customer.C_FIRST,random_string(8,16,rnd).c_str());
          strcpy(customer.C_STREET_1,random_string(10,20,rnd).c_str());
          strcpy(customer.C_STREET_2,random_string(10,20,rnd).c_str());
          strcpy(customer.C_CITY,random_string(10,20,rnd).c_str());
          strcpy(customer.C_STATE,random_string(2,2,rnd).c_str());
          strcpy(customer.C_ZIP,gen_zipcode(rnd).c_str());
          //TODO C_PHONE
         
          customer.C_SINCE = now;
          //TODO 10% GC 90% BC
          strcpy(customer.C_CREDIT, "GC");
          customer.C_CREDIT_LIM = 50000.00;
          customer.C_DISCOUNT = random_value(0.0000,0.50000);
          customer.C_BALANCE = -10.00;
          customer.C_YTD_PAYMENT = 10.00;
          customer.C_PAYMENT_CNT = 1;
          customer.C_DELIVERY_CNT = 0;
          strcpy(customer.C_DATA,random_string(300,500,rnd).c_str());
          
          key = customer.createKey();
          db_insert(Storage::CUSTOMER, key, {reinterpret_cast<char *>(&customer), sizeof(customer)});

          //CREATE History. 1 histories per customer.
          TPCC::History history{};
          
          history.H_C_ID = c;
          history.H_C_D_ID = history.H_D_ID = d;
          history.H_C_W_ID = history.H_W_ID = w;
          history.H_DATE = now;
          history.H_AMOUNT = 10.00;
          strcpy(history.H_DATA,random_string(12,24,rnd).c_str());

          key = std::to_string(hkg.get());
          db_insert(Storage::HISTORY, key, {reinterpret_cast<char *>(&history), sizeof(history)});

          //CREATE Order. 1 order per customer.
          TPCC::Order order{};
          order.O_ID = c;
          order.O_C_ID = c; //selected sequentially from a random permutation of [1 .. 3,000]
          order.O_D_ID = d;
          order.O_W_ID = w;
          order.O_ENTRY_D = now;
          if(order.O_ID<2101){
              order.O_CARRIER_ID = random_value(1,10);
          }else{
              order.O_CARRIER_ID = 0;
          }
          order.O_OL_CNT = random_value(5,15);
          order.O_ALL_LOCAL = 1;
          
          key = order.createKey();
          db_insert(Storage::ORDER, key, {reinterpret_cast<char *>(&order), sizeof(order)});

          //CREATE OrderLine. O_OL_CNT orderlines per order.
          for (size_t ol = 0; ol < order.O_OL_CNT; ol++) {
            TPCC::OrderLine order_line{};
            order_line.OL_O_ID = order.O_ID;
            order_line.OL_D_ID = d;
            order_line.OL_W_ID = w;
            order_line.OL_NUMBER = ol;
            order_line.OL_I_ID = random_value(1,100000);
            order_line.OL_SUPPLY_W_ID = w;
            if(order_line.OL_O_ID<2101){
                order_line.OL_DELIVERY_D = order.O_ENTRY_D;
            }else{
                order_line.OL_DELIVERY_D = 0;
            }
            order_line.OL_QUANTITY = 5;
            if(order_line.OL_O_ID<2101){
                order_line.OL_AMOUNT = 0.00;
            }else{
                order_line.OL_AMOUNT = random_value(0.01,99999.99);
            }
            order_line.OL_AMOUNT = 0.0;
            strcpy(order_line.OL_DIST_INFO,random_string(24,24,rnd).c_str());
            
            key = order_line.createKey();
            db_insert(Storage::ORDERLINE, key, {reinterpret_cast<char *>(&order_line), sizeof(order_line)});
          }

          //CREATE NewOrder 900 rows.
          //900 rows in the NEW-ORDER table corresponding to the last
          //3000-2100=900
          if (2100 < order.O_ID) {
            TPCC::NewOrder new_order{};
            new_order.NO_O_ID = order.O_ID;
            new_order.NO_D_ID = order.O_D_ID;
            new_order.NO_W_ID = order.O_W_ID;

            key = new_order.createKey();
            db_insert(Storage::NEWORDER, key, {reinterpret_cast<char *>(&new_order), sizeof(new_order)});
          }
        }
      }
    }
  }
}

}//namespace TPCC initializer



