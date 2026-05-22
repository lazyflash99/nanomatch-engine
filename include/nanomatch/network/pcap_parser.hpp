#pragma once

#include <cstdint>
#include <cstring>

namespace nanomatch {

/**
 * @brief Simple PCAP Parser to strip network headers from ITCH captures.
 * Standard PCAP format: Global Header -> [Packet Header -> Packet Data]
 */
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

    /**
     * @brief Get the offset to the first packet data.
     */
    static size_t get_global_header_size() { return sizeof(PcapGlobalHeader); }

    /**
     * @brief Parses headers and returns pointer to the ITCH payload.
     * Skips PCAP Packet Header (16b) + Ethernet (14b) + IP (20b) + UDP (8b) = 58 bytes
     */
    static const char* get_udp_payload(const char* packet_header_ptr, uint32_t& payload_len) {
        const auto* pkt_hdr = reinterpret_cast<const PcapPacketHeader*>(packet_header_ptr);
        
        // Offset to skip: PCAP Pkt Header (16) + Eth (14) + IP (20) + UDP (8)
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
