/*
TPC-C
http://www.tpc.org/tpc_documents_current_versions/pdf/tpc-c_v5.11.0.pdf

1.3 Table Layouts

*/

#include "tpcc_tables.hpp"

namespace TPCC::Initializer{

    bool load(size_t warehouse){
        std::time_t now = std::time(nullptr);

        {
            //CREATE Warehouses by single thread.
            for(size_t w=0;w<warehouse;w++){
                TPCC::Warehouse wh;
                wh.W_ID = w;
                wh.W_TAX = 1.5;
                wh.W_YTD = 1000'000'000;
                db.insert(warehouse.createKey(), &warehouse, sizeof(warehouse));
            }

        }
        {
            //CREATE  District
            for(size_t w=0;w<warehouse;w++){
                //10 districts per warehouse
                for(size_t d=0;d<10;d++){
                    TPCC::District district;
                    district.D_ID=d;
                    district.D_W_ID=w;
                    district.D_NEXT_O_ID=0;
                    district.D_TAX=1.5;
                    district.D_YTD=1000'000'000;

                    db.insert(warehouse.createKey(),&warehouse,sizeof(warehouse));

                    // CREATE Customer. 3000 customers per a district.
                    for(size_t c=0;c<3000;c++){
                        TPCC::Custormer customer;
                        customer.C_ID = c;
                        customer.C_D_ID=d;
                        customer.C_W_ID=w;
                        strcpy(customer.C_LAST, "SUZUKI");
                        customer.C_SINCE=now;
                        strcpy(customer.C_CREDIT,"GC");
                        customer.C_DISCOUNT=0.1;
                        customer.C_BALANCE=-10.00;
                        customer.C_YTD_PAYMENT  = 10.00;
                        customer.C_PAYMENT_CNT=1;
                        customer.C_PAYMENT_CNT  = 1;
                        customer.C_DELIVERY_CNT = 0;

                        db.insert(customer.createKey(),&customer,sizeof(customer));


                        //CREATE History. 1 histories per customer.
                        TPCC::History history;
                        history.H_C_ID = c;
                        history.H_C_D_ID = history.H_D_ID=d;
                        history.H_C_W_ID = w;
                        history.H_DATE=now;

                        db.insert(history.createKey(),&history,sizeof(history));



                        //CREATE Order. 1 order per customer.
                        TPCC::Order order;
                        order.O_ID=c;
                        order.O_C_ID=c;
                        order.O_D_ID=d;
                        order.O_W_ID=w;
                        order.O_ENTRY_D=now;
                        
                        db.insert(order.createKey(),&order,sizeof(order));

                        //CREATE OrderLine. O_OL_CNT orderlines per order.
                        for(size_t ol=0;ol<order.O_OL_CNT;ol++){
                            TPCC::OrderLine order_line;
                            order_line.OL_O_ID=order.O_ID;
                            order_line.OL_D_ID=d;
                            order_line.OL_NUMBER=ol;
                            order_line.OL_I_ID=1;
                            order_line.OL_SUPPLY_W_ID=w;
                            order_line.OL_DELIVERY_D=now;
                            order_line.OL_AMOUNT=0.0;

                            db.insert(order_line.createKey(),&order_line,sizeof(order_line));

                        }


                        //CREATE NewOrder 900 rows.
                        //900 rows in the NEW-ORDER table corresponding to the last
                        //3000-2100=900
                        if(2100<order.O_ID){
                            TPCC::NewOrder new_order;
                            new_order.NO_O_ID = order.O_ID;
                            new_order.NO_D_ID = order.O_D_ID;
                            new_order.NO_W_ID = order.O_W_ID;

                            db.insert(new_order.createKey(),&new_order,sizeof(new_order));
                        }                        

                    }

                }
            }

        }

    }

}//namespace TPCC initializer
