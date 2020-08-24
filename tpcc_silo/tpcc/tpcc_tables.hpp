/**
 * @file tpcc_tables.hpp
 * @details
 * TPC-C
 * http://www.tpc.org/tpc_documents_current_versions/pdf/tpc-c_v5.11.0.pdf
 * 1.3 Table Layouts
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <string>
#include <cassert>
#include <sstream>
#include "atomic_wrapper.h"
#include "cpu.h"
#include "tpcc_util.hpp"


template<size_t N>
struct SimpleKey {
  char data[N]; // not null-terminated.

  char *ptr() { return &data[0]; }

  const char *ptr() const { return &data[0]; }

  [[nodiscard]] std::string_view view() const {
    return std::string_view(&data[0], N);
  }

  int compare(const SimpleKey& rhs) const {
    return ::memcmp(data, rhs.data, N);
  }
  bool operator<(const SimpleKey& rhs) const {
    return compare(rhs) < 0;
  }
  bool operator==(const SimpleKey& rhs) const {
    return compare(rhs) == 0;
  }
};


namespace TPCC {

struct Warehouse {
  alignas(CACHE_LINE_SIZE)
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
  //key size is 8 bytes.
  static void CreateKey(uint16_t w_id, char *out) {
    ::memset(out, 0, 6);
    assign_as_bigendian(w_id, &out[6]);
  }

  void createKey(char *out) const { return CreateKey(W_ID, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct District {
  alignas(CACHE_LINE_SIZE)
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
  //key size is 8.
  static void CreateKey(uint16_t w_id, uint8_t d_id, char *out) {
    ::memset(out, 0, 5);
    assign_as_bigendian(w_id, &out[5]);
    assign_as_bigendian(d_id, &out[7]);
  }

  void createKey(char *out) const { return CreateKey(D_W_ID, D_ID, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct Customer {
  //(C_W_ID, C_D_ID) Foreign Key, references (D_W_ID, D_ID)
  alignas(CACHE_LINE_SIZE)
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
  std::uint64_t C_SINCE;//date and time
  char C_CREDIT[3];   //"GC"=good, "BC"=bad
  double C_CREDIT_LIM;
  double C_DISCOUNT;
  double C_BALANCE;
  double C_YTD_PAYMENT;
  uint16_t C_PAYMENT_CNT;
  uint16_t C_DELIVERY_CNT;
  char C_DATA[501];


  struct Key {
    uint16_t w_id;
    uint8_t d_id;
    uint32_t c_id;

    constexpr static size_t required_size() { return 8; }

    size_t parse(const char* in) {
      parse_bigendian(&in[0], w_id);
      parse_bigendian(&in[3], d_id);
      parse_bigendian(&in[4], c_id);
      return required_size();
    }
    size_t create(char* out) const {
      assign_as_bigendian(w_id, &out[0]);
      out[2] = 0;
      assign_as_bigendian(d_id, &out[3]);
      assign_as_bigendian(c_id, &out[4]);
      return required_size();
    }
    SimpleKey<8> create() const {
      SimpleKey<8> ret;
      create(ret.ptr());
      return ret;
    }
    std::string pretty_str() const {
      char buf[128];
      ::snprintf(buf, sizeof(buf), "Customer_Key: w_id %u  d_id %u  c_id %u", w_id, d_id, c_id);
      return std::string(buf);
    }
  };

  struct CLastKey {
    uint16_t c_w_id;
    uint8_t c_d_id;
    char* c_last;

    constexpr static size_t required_size() { return sizeof(C_W_ID) + sizeof(C_D_ID) + sizeof(C_LAST); }

    /**
     * CAUSION: c_last is just a pointer. Set it approprieately before calling parse().
     */
    size_t parse(const char* in) {
      parse_bigendian(&in[0], c_w_id);
      parse_bigendian(&in[2], c_d_id);
      size_t len = copy_cstr(c_last, &in[3], sizeof(C_LAST));
      return sizeof(c_w_id) + sizeof(c_d_id) + len;
    }
    // out buffer size must no less than sizeof(CLastKey).
    size_t create(char* out) {
      assign_as_bigendian(c_w_id, &out[0]);
      assign_as_bigendian(c_d_id, &out[2]);
      size_t len = copy_cstr(&out[3], c_last, sizeof(C_LAST));
      return sizeof(c_w_id) + sizeof(c_d_id) + len;
    }
    std::string pretty_str() const {
      char buf[128];
      ::snprintf(buf, sizeof(buf), "Customer_CLastKey: c_w_id %u  c_d_id %u  c_last %s\n", c_w_id, c_d_id, c_last);
      return std::string(buf);
    }
  };


  //Primary Key: (C_W_ID, C_D_ID, C_ID)
  //key size is 8 bytes.
  static void CreateKey(uint16_t w_id, uint8_t d_id, uint32_t c_id, char *out) {
    Key key{w_id, d_id, c_id};
    key.create(out);
  }

  void createKey(char *out) const { return CreateKey(C_W_ID, C_D_ID, C_ID, out); }


  //Secondary Key: (C_W_ID, C_D_ID, C_LAST)
  //key length is variable. (maximum length is maxLenOfSecondaryKey()).
  //out buffer will not be null-terminated.
  static std::string_view CreateSecondaryKey(uint16_t w_id, uint8_t d_id, char* c_last, char* out) {
    CLastKey key{w_id, d_id, c_last};
    size_t len = key.create(out);
    return std::string_view(out, len);
  }

  std::string_view createSecondaryKey(char* out){ return CreateSecondaryKey(C_W_ID, C_D_ID, &C_LAST[0], out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct History {
  // Primary Key: none
  // (H_C_W_ID, H_C_D_ID, H_C_ID) Foreign Key, references (C_W_ID, C_D_ID, C_ID)
  // (H_W_ID, H_D_ID) Foreign Key, references (D_W_ID, D_ID)

  alignas(CACHE_LINE_SIZE)
  std::uint32_t H_C_ID; //96,000 unique IDs
  std::uint8_t H_C_D_ID;// 20 unique IDs
  std::uint16_t H_C_W_ID; // 2*W unique IDs
  std::uint8_t H_D_ID; //20 unique IDs
  std::uint16_t H_W_ID; // 2*W unique IDs
  std::uint64_t H_DATE; //date and time
  double H_AMOUNT; //signed numeric(6, 2)
  char H_DATA[25]; // variable text, size 24 Miscellaneous information

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};


class HistoryKeyGenerator {
public:
  union {
    alignas(CACHE_LINE_SIZE)
    std::uint64_t key_;
    struct {
      std::uint64_t counter_: 47;
      std::uint64_t at_work_: 1; // 0 at initial load, 1 at work.
      std::uint64_t id_: 16;
    };
    // upper bits are thread_id in little endian architecture.
  };

  std::uint64_t get_raw() {
    return ccbench::fetchAdd(key_, 1);
  }

  SimpleKey<8> get_as_simple_key() {
    SimpleKey<8> ret{};
    assign_as_bigendian<uint64_t>(get_raw(), ret.ptr());
    return ret;
  }

  void init(uint16_t id, bool at_work) {
    counter_ = 0;
    at_work_ = at_work ? 1 : 0;
    id_ = id;
  }
};


struct NewOrder {
  //(NO_W_ID, NO_D_ID, NO_O_ID) Foreign Key, references (O_W_ID, O_D_ID, O_ID)
  alignas(CACHE_LINE_SIZE)
  std::uint32_t NO_O_ID; //10,000,000 unique IDs
  std::uint8_t NO_D_ID; //20 unique IDs
  std::uint16_t NO_W_ID; //2*W unique IDs

  //Primary Key: (NO_W_ID, NO_D_ID, NO_O_ID)
  //key size is 8 bytes.
  static void CreateKey(uint16_t w_id, uint8_t d_id, uint32_t o_id, char *out) {
    assign_as_bigendian(w_id, &out[0]);
    out[2] = 0;
    assign_as_bigendian(d_id, &out[3]);
    assign_as_bigendian(o_id, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(NO_W_ID, NO_D_ID, NO_O_ID, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct Order {
  //(O_W_ID, O_D_ID, O_C_ID) Foreign Key, references (C_W_ID, C_D_ID, C_ID)
  alignas(CACHE_LINE_SIZE)
  std::uint32_t O_ID; //10,000,000 unique IDs
  std::uint8_t O_D_ID; // 20 unique IDs
  std::uint16_t O_W_ID; // 2*W unique IDs
  std::uint32_t O_C_ID; //96,000 unique IDs
  std::uint64_t O_ENTRY_D; //date and time
  std::uint32_t O_CARRIER_ID; // unique IDs, or null
  std::uint8_t O_OL_CNT; //numeric(2) Count of Order-Lines
  std::uint8_t O_ALL_LOCAL; //numeric(1)

  //Primary Key: (O_W_ID, O_D_ID, O_ID)
  //key size is 8 bytes.
  static void CreateKey(uint16_t w_id, uint8_t d_id, uint32_t o_id, char *out) {
    assign_as_bigendian(w_id, &out[0]);
    out[2] = 0;
    assign_as_bigendian(d_id, &out[3]);
    assign_as_bigendian(o_id, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(O_W_ID, O_D_ID, O_ID, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct OrderLine {
  //(OL_W_ID, OL_D_ID, OL_O_ID) Foreign Key, references (O_W_ID, O_D_ID, O_ID)
  //(OL_SUPPLY_W_ID, OL_I_ID) Foreign Key, references (S_W_ID, S_I_ID)
  alignas(CACHE_LINE_SIZE)
  std::uint32_t OL_O_ID;// 10,000,000 unique IDs
  std::uint8_t OL_D_ID;// 20 unique IDs
  std::uint16_t OL_W_ID;// 2*W unique IDs
  std::uint8_t OL_NUMBER;// 15 unique IDs
  std::uint32_t OL_I_ID;// 200,000 unique IDs
  std::uint16_t OL_SUPPLY_W_ID;// 2*W unique IDs
  std::uint64_t OL_DELIVERY_D;// date and time, or null
  std::uint8_t OL_QUANTITY;// numeric(2)
  double OL_AMOUNT;// signed numeric(6, 2)
  char OL_DIST_INFO[25];// fixed text, size 24

  //Primary Key: (OL_W_ID, OL_D_ID, OL_O_ID, OL_NUMBER)
  //key size is 8 bytes.
  static void CreateKey(uint16_t w_id, uint8_t d_id, uint32_t o_id, uint8_t ol_num, char *out) {
    assign_as_bigendian(w_id, &out[0]);
    assign_as_bigendian(d_id, &out[2]);
    assign_as_bigendian(o_id, &out[3]);
    assign_as_bigendian(ol_num, &out[7]);
  }

  void createKey(char *out) const { CreateKey(OL_W_ID, OL_D_ID, OL_O_ID, OL_NUMBER, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct Item {
  alignas(CACHE_LINE_SIZE)
  std::uint32_t I_ID; //200,000 unique IDs 100,000 items are populated
  std::uint32_t I_IM_ID; //200,000 unique IDs Image ID associated to Item
  char I_NAME[25]; //variable text, size 24
  double I_PRICE; //numeric(5, 2)
  char I_DATA[51]; //variable text, size 50 Brand information

  //Primary Key: I_ID
  //key size is 8 bytes.
  static void CreateKey(uint32_t i_id, char *out) {
    ::memset(&out[0], 0, 4);
    assign_as_bigendian(i_id, &out[4]);
  }

  void createKey(char *out) const { CreateKey(I_ID, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct Stock {
  //S_W_ID Foreign Key, references W_ID
  //S_I_ID Foreign Key, references I_ID
  alignas(CACHE_LINE_SIZE)
  std::uint32_t S_I_ID; //200,000 unique IDs 100,000 populated per warehouse
  std::uint16_t S_W_ID; //2*W unique IDs
  int16_t S_QUANTITY; //signed numeric(4)
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
  std::uint32_t S_YTD; // numeric(8)
  std::uint16_t S_ORDER_CNT; // numeric(4)
  std::uint16_t S_REMOTE_CNT; // numeric(4)
  char S_DATA[51]; // variable text, size 50

  // Primary Key: (S_W_ID, S_I_ID) composite.
  // key size is 8 bytes.
  static void CreateKey(uint16_t w_id, uint32_t i_id, char *out) {
    ::memset(&out[0], 0, 2);
    assign_as_bigendian(w_id, &out[2]);
    assign_as_bigendian(i_id, &out[4]);
  }

  void createKey(char *out) const { CreateKey(S_W_ID, S_I_ID, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

}
