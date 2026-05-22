import struct
import random
import argparse
import os
import time

# --- FORMAT DEFINITIONS ---
# ITCH: [Type:1][OrderID:8][Side:1][Qty:4][Price:8][InstrumentID:4] = 26 bytes + 1 type byte
ITCH_FORMAT = ">B Q c I q I"

# PCAP Headers
GLOBAL_HDR = struct.pack("<I H H i I I I", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
# Ethernet(14) + IP(20) + UDP(8) = 42 bytes overhead
# PCAP Pkt Hdr: [ts_sec:4][ts_usec:4][incl_len:4][orig_len:4]
PKT_HDR_FORMAT = "<I I I I"

def wrap_in_pcap(payload):
    # Dummy Network Headers: Eth(14) + IP(20) + UDP(8)
    dummy_net = b'\x00' * 42 
    full_payload = dummy_net + payload
    incl_len = len(full_payload)
    pkt_hdr = struct.pack(PKT_HDR_FORMAT, int(time.time()), 0, incl_len, incl_len)
    return pkt_hdr + full_payload

def generate_data(filename, num_messages, scenario, use_pcap, instrument_id=1):
    print(f"Scenario: {scenario.upper()} | Output: {filename} | PCAP: {use_pcap}")
    
    base_price = 10000
    order_id_counter = 1
    active_orders = []
    
    with open(filename, "wb") as f:
        if use_pcap:
            f.write(GLOBAL_HDR)

        for i in range(num_messages):
            # Scenario Logic
            if scenario == "crash":
                msg_type = b'M' if random.random() < 0.3 else b'A'
                side = b'S' if msg_type == b'M' else b'B' # Heavy selling
            elif scenario == "hft":
                msg_type = b'X' if random.random() < 0.6 else b'A'
                side = b'B' if random.random() > 0.5 else b'S'
            else: # normal
                rand = random.random()
                msg_type = b'A' if rand < 0.7 else (b'M' if rand < 0.8 else b'X')
                side = b'B' if random.random() > 0.5 else b'S'

            # Build Message
            if msg_type == b'X' and not active_orders:
                msg_type = b'A' # Fallback

            if msg_type == b'A':
                offset = random.randint(1, 50)
                price = base_price - offset if side == b'B' else base_price + offset
                qty = random.randint(1, 10) * 10
                payload = struct.pack(ITCH_FORMAT, ord(msg_type), order_id_counter, side, qty, price, instrument_id)
                active_orders.append((order_id_counter, side, price))
                order_id_counter += 1
            elif msg_type == b'M':
                qty = random.randint(100, 1000) # Market orders are huge in crash
                payload = struct.pack(ITCH_FORMAT, ord(msg_type), order_id_counter, side, qty, 0, instrument_id)
                order_id_counter += 1
            else: # Cancel
                oid, s, p = random.choice(active_orders)
                payload = struct.pack(ITCH_FORMAT, ord('X'), oid, s, 0, p, instrument_id)
                active_orders.remove((oid, s, p))

            if use_pcap:
                f.write(wrap_in_pcap(payload))
            else:
                f.write(payload)

            if i % 10000 == 0 and i > 0:
                print(f"  ... {i} messages generated")
                if len(active_orders) > 5000: active_orders = active_orders[-2000:]

    print(f"Success! Final size: {os.path.getsize(filename) / (1024*1024):.2f} MB")

def generate_csv(filename, num_messages, scenario):
    print(f"Scenario: {scenario.upper()} | Output: {filename} | Format: CSV")
    base_price = 10000
    order_id_counter = 1
    active_orders = []
    
    with open(filename, "w") as f:
        f.write("type,order_id,side,quantity,price,instrument_id\n")
        for i in range(num_messages):
            if scenario == "crash":
                msg_type = 'M' if random.random() < 0.3 else 'A'
                side = 'S' if msg_type == 'M' else 'B'
            elif scenario == "hft":
                msg_type = 'X' if random.random() < 0.6 else 'A'
                side = 'B' if random.random() > 0.5 else 'S'
            else:
                rand = random.random()
                msg_type = 'A' if rand < 0.7 else ('M' if rand < 0.8 else 'X')
                side = 'B' if random.random() > 0.5 else 'S'

            if msg_type == 'X' and not active_orders: msg_type = 'A'

            if msg_type == 'A':
                price = base_price + (random.randint(1, 50) * (1 if side == 'S' else -1))
                qty = random.randint(1, 10) * 10
                f.write(f"A,{order_id_counter},{side},{qty},{price},1\n")
                active_orders.append((order_id_counter, side, price))
                order_id_counter += 1
            elif msg_type == 'M':
                qty = random.randint(100, 1000)
                f.write(f"M,{order_id_counter},{side},{qty},0,1\n")
                order_id_counter += 1
            else:
                oid, s, p = random.choice(active_orders)
                f.write(f"X,{oid},{s},0,{p},1\n")
                active_orders.remove((oid, s, p))
            
            if i % 10000 == 0 and i > 0:
                if len(active_orders) > 5000: active_orders = active_orders[-2000:]

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="test_data.bin")
    parser.add_argument("--count", type=int, default=1000000)
    parser.add_argument("--scenario", choices=["normal", "crash", "hft"], default="normal")
    parser.add_argument("--pcap", action="store_true", help="Wrap in PCAP/UDP headers")
    parser.add_argument("--format", choices=["binary", "csv"], default="binary")
    args = parser.parse_args()
    
    if args.format == "csv":
        generate_csv(args.output, args.count, args.scenario)
    else:
        generate_data(args.output, args.count, args.scenario, args.pcap)
