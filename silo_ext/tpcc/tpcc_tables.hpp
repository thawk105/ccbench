#pragma once
/*
 * TPC-C
 * http://www.tpc.org/tpc_documents_current_versions/pdf/tpc-c_v5.11.0.pdf
 * 1.3 Table Layouts
 */

#include <cstdint>
#include <chrono>
#include <mutex>
#include <string>


namespace TPCC {

struct Warehouse {
  //constexpr const static char* kPrefix = "warehouse";
  std::uint16_t W_ID; //2*W unique IDs
  char W_NAME[11];
  char W_STREET_1[21];
  char W_STREET_2[21];
  char W_CITY[21];
  char W_STATE[3];
  char W_ZIP[10];
  double W_TAX;
  double W_YTD;

  //Primary Key: W_ID
  static std::string CreateKey(size_t w_id) {
    //return std::string(kPrefix + std::to_string(w_id));
    return std::string(std::to_string(w_id));
  }

  std::string createKey() { return CreateKey(W_ID); }
};

struct District {
  constexpr const static char *kPrefix = "district";

  std::uint8_t D_ID; //20 unique IDs
  std::uint16_t D_W_ID; //2*W unique IDs D_W_ID Foreign Key, references W_ID
  char D_NAME[11];
  char D_STREET_1[21];
  char D_STREET_2[21];
  char D_CITY[21];
  char D_STATE[3];
  char D_ZIP[10];
  double D_TAX; //signed numeric(4,4)
  double D_YTD; //signed numeric(12,2)
  std::uint32_t D_NEXT_O_ID; //10,000,000 unique IDs

  //Primary Key: (D_W_ID, D_ID)
  static std::string CreateKey(size_t w, size_t d) {
    return std::string(Warehouse::CreateKey(w) + kPrefix + std::to_string(d));
  }

  std::string createKey() { return CreateKey(D_W_ID, D_ID); }

};

struct Customer {
  constexpr const static char *kPrefix = "customer";

  //(C_W_ID, C_D_ID) Foreign Key, references (D_W_ID, D_ID)
  std::uint32_t C_ID; //96,000 unique IDs
  std::uint8_t C_D_ID; //20 unique IDs
  std::uint16_t C_W_ID; //2*W unique IDs
  char C_FIRST[17];
  char C_MIDDLE[3];
  char C_LAST[17]; //the customer's last name
  char C_STREET_1[21];
  char C_STREET_2[21];
  char C_CITY[21];
  char C_STATE[3];
  char C_ZIP[10];
  char C_PHONE[17];
  std::time_t C_SINCE;//date and time
  char C_CREDIT[3];   //"GC"=good, "BC"=bad
  double C_CREDIT_LIM;
  double C_DISCOUNT;
  double C_BALANCE;
  double C_YTD_PAYMENT;
  double C_PAYMENT_CNT;
  double C_DELIVERY_CNT;
  char C_DATA[501];

  //Primary Key: (C_W_ID, C_D_ID, C_ID)
  static std::string CreateKey(size_t w, size_t d, size_t c) {
    return std::string(District::CreateKey(w, d) + kPrefix + std::to_string(c));
  }

  std::string createKey() { return CreateKey(C_W_ID, C_D_ID, C_ID); }
};

struct History {

  // Primary Key: none
  // (H_C_W_ID, H_C_D_ID, H_C_ID) Foreign Key, references (C_W_ID, C_D_ID, C_ID)
  // (H_W_ID, H_D_ID) Foreign Key, references (D_W_ID, D_ID)

  uint32_t H_C_ID; //96,000 unique IDs
  uint8_t H_C_D_ID;// 20 unique IDs
  uint16_t H_C_W_ID; // 2*W unique IDs
  uint8_t H_D_ID; //20 unique IDs
  uint16_t H_W_ID; // 2*W unique IDs
  std::time_t H_DATE; //date and time
  double H_AMOUNT; //signed numeric(6, 2)
  char H_DATA[25]; // variable text, size 24 Miscellaneous information


};


class HistoryKeyGenerator {
public:
  union {
    std::uint64_t key_;
    struct {
      std::uint64_t counter_: 55;
      bool at_load_: 1;
      /**
       * @details If at_load_ is true, this is warehosue_id. If not, warehouse_id_ is thread_id.
       */
      std::uint8_t warehouse_id_: 8;
    };
    // upper bits are thread_id in little endian architecture.
  };

  std::uint64_t get() {
    mtx_hkg_.lock();
    ++counter_;
    std::uint64_t rt_key{key_};
    mtx_hkg_.unlock();
    return rt_key;
  }

  void init(std::uint8_t warehouse_id, bool at_load) {
    counter_ = 0;
    at_load_ = at_load;
    warehouse_id_ = warehouse_id;
  }

private:
  std::mutex mtx_hkg_;
};


struct NewOrder {
  constexpr const static char *kPrefix = "neworder";
  //(NO_W_ID, NO_D_ID, NO_O_ID) Foreign Key, references (O_W_ID, O_D_ID, O_ID)
  std::uint32_t NO_O_ID; //10,000,000 unique IDs
  std::uint8_t NO_D_ID; //20 unique IDs
  std::uint16_t NO_W_ID; //2*W unique IDs

  //Primary Key: (NO_W_ID, NO_D_ID, NO_O_ID)
  static std::string CreateKey(size_t w, size_t d, size_t o) {
    return std::string(kPrefix + std::to_string(w) + "-" + std::to_string(d) + "-" + std::to_string(o));
  }

  std::string createKey() { return CreateKey(NO_W_ID, NO_D_ID, NO_O_ID); }

};

struct Order {
  constexpr const static char *kPrefix = "order";
  //(O_W_ID, O_D_ID, O_C_ID) Foreign Key, references (C_W_ID, C_D_ID, C_ID)
  std::uint32_t O_ID; //10,000,000 unique IDs
  std::uint8_t O_D_ID; // 20 unique IDs
  std::uint16_t O_W_ID; // 2*W unique IDs
  std::uint32_t O_C_ID; //96,000 unique IDs
  std::time_t O_ENTRY_D; //date and time
  std::uint32_t O_CARRIER_ID; // unique IDs, or null
  double O_OL_CNT; //numeric(2) Count of Order-Lines
  double O_ALL_LOCAL; //numeric(1)

  //Primary Key: (O_W_ID, O_D_ID, O_ID)
  static std::string CreateKey(size_t w, size_t d, size_t i) {
    return std::string(kPrefix + std::to_string(w) + "-" + std::to_string(d) + "-" + std::to_string(i));
  }

  std::string createKey() { return CreateKey(O_W_ID, O_D_ID, O_ID); }
};

struct OrderLine {
  constexpr const static char *kPrefix = "orderline";

  //(OL_W_ID, OL_D_ID, OL_O_ID) Foreign Key, references (O_W_ID, O_D_ID, O_ID)
  //(OL_SUPPLY_W_ID, OL_I_ID) Foreign Key, references (S_W_ID, S_I_ID)
  std::uint32_t OL_O_ID;// 10,000,000 unique IDs
  std::uint8_t OL_D_ID;// 20 unique IDs
  std::uint16_t OL_W_ID;// 2*W unique IDs
  std::uint8_t OL_NUMBER;// 15 unique IDs
  std::uint32_t OL_I_ID;// 200,000 unique IDs
  std::uint16_t OL_SUPPLY_W_ID;// 2*W unique IDs
  std::time_t OL_DELIVERY_D;// date and time, or null
  double OL_QUANTITY;// numeric(2)
  double OL_AMOUNT;// signed numeric(6, 2)
  char OL_DIST_INFO[25];// fixed text, size 24

  //Primary Key: (OL_W_ID, OL_D_ID, OL_O_ID, OL_NUMBER)
  static std::string CreateKey(size_t w, size_t d, size_t o, size_t n) {
    return std::string(
            kPrefix + std::to_string(w) + "-" + std::to_string(d) + "-" + std::to_string(o) + "-" + std::to_string(n));
  }

  std::string createKey() {
    return CreateKey(OL_W_ID, OL_D_ID, OL_O_ID, OL_NUMBER);
  }
};

struct Item {
  constexpr const static char *kPrefix = "item";

  std::uint32_t I_ID; //200,000 unique IDs 100,000 items are populated
  std::uint32_t I_IM_ID; //200,000 unique IDs Image ID associated to Item
  char I_NAME[25]; //variable text, size 24
  double I_PRICE; //numeric(5, 2)
  char I_DATA[51]; //variable text, size 50 Brand information

  //Primary Key: I_ID
  static std::string CreateKey(size_t i) {
    return std::string(kPrefix + std::to_string(i));
  }

  std::string createKey() {
    return CreateKey(I_ID);
  }

};

struct Stock {
  constexpr const static char *kPrefix = "stock";

  //S_W_ID Foreign Key, references W_ID
  //S_I_ID Foreign Key, references I_ID
  std::uint16_t S_I_ID; //200,000 unique IDs 100,000 populated per warehouse
  std::uint8_t S_W_ID; //2*W unique IDs
  double S_QUANTITY; //signed numeric(4)
  char S_DIST_01[25]; //fixed text, size 24
  char S_DIST_02[25]; //fixed text, size 24
  char S_DIST_03[25]; //fixed text, size 24
  char S_DIST_04[25]; // fixed text, size 24
  char S_DIST_05[25]; // fixed text, size 24
  char S_DIST_06[25]; // fixed text, size 24
  char S_DIST_07[25]; // fixed text, size 24
  char S_DIST_08[25]; // fixed text, size 24
  char S_DIST_09[25]; // fixed text, size 24
  char S_DIST_10[25]; // fixed text, size 24
  double S_YTD; // numeric(8)
  double S_ORDER_CNT; // numeric(4)
  double S_REMOTE_CNT; // numeric(4)
  char S_DATA[51]; // variable text, size 50

  // Primary Key: (S_W_ID, S_I_ID) composite.
  static std::string CreateKey(size_t w, size_t i) {
    return std::string(Warehouse::CreateKey(w) + kPrefix + std::to_string(i));
  }

  std::string createKey() { return CreateKey(S_W_ID, S_I_ID); }
};

}
