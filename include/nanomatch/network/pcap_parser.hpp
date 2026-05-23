#pragma once

#include <cstdint>
#include <cstring>

namespace nanomatch {

class PCAPParser {
public:
    struct PcapGlobalHeader {
        uint32_t magic_number;
        uint16_t version_major;
        uint16_t version_minor;
        int32_t  thiszone;
        uint32_t sigfigs;
        uint32_t snaplen;
        uint32_t network;
    } __attribute__((packed));

    struct PcapPacketHeader {
        uint32_t ts_sec;
        uint32_t ts_usec;
        uint32_t incl_len;
        uint32_t orig_len;
    } __attribute__((packed));

    static size_t get_global_header_size() { return sizeof(PcapGlobalHeader); }

    static const char* get_udp_payload(const char* packet_header_ptr, uint32_t& payload_len) {
        const auto* pkt_hdr = reinterpret_cast<const PcapPacketHeader*>(packet_header_ptr);
        constexpr size_t total_network_overhead = sizeof(PcapPacketHeader) + 14 + 20 + 8;
        if (pkt_hdr->incl_len < (14 + 20 + 8)) return nullptr;
        payload_len = pkt_hdr->incl_len - (14 + 20 + 8);
        return packet_header_ptr + total_network_overhead;
    }

    static size_t get_next_packet_offset(const char* packet_header_ptr) {
        const auto* pkt_hdr = reinterpret_cast<const PcapPacketHeader*>(packet_header_ptr);
        return sizeof(PcapPacketHeader) + pkt_hdr->incl_len;
    }
};

} // namespace nanomatch
