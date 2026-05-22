#include "nanomatch/core/order_book.hpp"
#include "nanomatch/utils/mapped_file.hpp"
#include "nanomatch/network/itch_parser.hpp"
#include "nanomatch/network/pcap_parser.hpp"
#include "nanomatch/network/csv_parser.hpp"
#include "nanomatch/utils/object_pool.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <string_view>

using namespace nanomatch;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <data_file.bin/pcap/csv>\n";
        return 1;
    }

    try {
        MappedFile file(argv[1]);
        const char* ptr = file.data();
        const char* end = ptr + file.size();

        auto order_pool = std::make_unique<ObjectPool<Order, 100000>>();
        auto report_queue = std::make_unique<SPSCQueue<TradeReport, 1024>>();
        auto ob = std::make_unique<OrderBook<1024, 100000>>(1, report_queue.get());
        
        std::atomic<bool> running{true};

        std::thread logger([&]() {
            while (running || report_queue->available_for_pop()) {
                auto report = report_queue->pop();
                if (report) {
                    std::cout << "[TRADE] ID:" << report->trade_id 
                              << " Price:" << report->price 
                              << " Qty:" << report->quantity << "\n";
                } else {
                    std::this_thread::yield();
                }
            }
        });

        bool is_pcap = (*reinterpret_cast<const uint32_t*>(ptr) == 0xa1b2c3d4 || 
                        *reinterpret_cast<const uint32_t*>(ptr) == 0xd4c3b2a1);
        
        bool is_csv = (std::string_view(ptr, 4) == "type");

        size_t processed = 0;

        if (is_csv) {
            std::string_view content(ptr, end - ptr);
            size_t start = 0, pos = 0;
            while ((pos = content.find('\n', start)) != std::string_view::npos) {
                std::string_view line = content.substr(start, pos - start);
                CSVParser::Row row;
                if (CSVParser::parse_row(line, row)) {
                    Order* order = order_pool->acquire();
                    if (order) {
                        order->order_id = row.order_id;
                        order->price = row.price;
                        order->quantity = row.quantity;
                        order->instrument_id = 1;
                        order->side = (row.side == 'B') ? Side::BUY : Side::SELL;
                        order->type = (row.type == 'M') ? OrderType::MARKET : OrderType::LIMIT;
                        ob->add_order(order);
                        if (row.type == 'M' || order->quantity == 0) {
                            order_pool->release(order);
                        }
                    }
                    processed++;
                }
                start = pos + 1;
            }
        } else {
            if (is_pcap) ptr += PCAPParser::get_global_header_size();

            while (ptr < end) {
                const char* payload = ptr;
                uint32_t payload_len = 0;

                if (is_pcap) {
                    payload = PCAPParser::get_udp_payload(ptr, payload_len);
                    if (!payload) break;
                    ptr += PCAPParser::get_next_packet_offset(ptr);
                } else {
                    payload_len = static_cast<uint32_t>(end - ptr);
                }

                const char* msg_ptr = payload;
                const char* msg_end = payload + payload_len;

                while (msg_ptr + 1 <= msg_end) {
                    char msg_type = *msg_ptr;
                    msg_ptr++;

                    if (msg_type == 'A' || msg_type == 'M' || msg_type == 'X') {
                        const auto* msg = ITCHParser::parse_add_order(msg_ptr);
                        if (msg_type == 'X') {
                            Order* canceled = ob->cancel_order(msg->order_id);
                            if (canceled) order_pool->release(canceled);
                        } else {
                            Order* order = order_pool->acquire();
                            if (order) {
                                order->order_id = msg->order_id;
                                order->price = (msg_type == 'M') ? 0 : msg->price;
                                order->quantity = msg->quantity;
                                order->instrument_id = msg->instrument_id;
                                order->side = ITCHParser::convert_side(msg->side);
                                order->type = (msg_type == 'M') ? OrderType::MARKET : OrderType::LIMIT;
                                ob->add_order(order);
                                if (msg_type == 'M' || order->quantity == 0) {
                                    order_pool->release(order);
                                }
                            }
                        }
                        msg_ptr += sizeof(ITCHParser::AddOrderMsg);
                        processed++;
                    } else {
                        if (!is_pcap) msg_ptr = msg_end;
                        else break;
                    }
                }
                if (!is_pcap) break;
            }
        }

        std::cout << "Processed " << processed << " messages.\n";
        running = false;
        logger.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
