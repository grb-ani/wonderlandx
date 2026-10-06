#!/usr/bin/env uv run
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyzmq>=25.1"]
# ///

import zmq
import struct
import binascii
from pprint import pprint


def hex_dump(data: bytes) -> str:
    return " ".join(f"{b:02x}" for b in data)


# ------------------------------------------------------------
# WonderlandX Event Parser (E packets)
# ------------------------------------------------------------
def parse_event_packet(data: bytes):
    offset = 0

    if not data or data[0] != ord('E'):
        return {"error": "Not an event packet"}

    offset += 1

    try:
        # Event name
        event_len = struct.unpack_from(">I", data, offset)[0]
        offset += 4

        event_name = data[offset:offset + event_len].decode("utf-8", errors="replace")
        offset += event_len

        # Arg count
        arg_count = struct.unpack_from(">I", data, offset)[0]
        offset += 4

        args = []

        for _ in range(arg_count):
            arg_type = data[offset]
            offset += 1

            # uint32 / int32
            if arg_type in (0x01, 0x02):
                raw = data[offset:offset + 4]
                offset += 4
                value = struct.unpack(">I", raw)[0]
                args.append({
                    "type": "uint" if arg_type == 0x01 else "sint",
                    "value": value,
                })

            # string
            elif arg_type == 0x03:
                strlen = struct.unpack_from(">I", data, offset)[0]
                offset += 4

                raw = data[offset:offset + strlen]
                offset += strlen

                decoded = raw.decode("utf-8", errors="replace")
                args.append({
                    "type": "string",
                    "len": strlen,
                    "raw": raw,
                    "value": decoded,
                })

            else:
                return {
                    "error": f"Unknown arg type {arg_type}",
                    "event": event_name,
                }

        return {
            "packet_type": "EVENT",
            "event": event_name,
            "args": args,
        }

    except Exception as e:
        return {"error": f"Parse error: {e}"}


# ------------------------------------------------------------
# WonderlandX Function Packet Parser (F packets)
# ------------------------------------------------------------
def parse_function_packet(data: bytes):
    offset = 1  # skip 'F'

    try:
        client_id = struct.unpack_from(">I", data, offset)[0]
        offset += 4

        packet_id = struct.unpack_from(">I", data, offset)[0]
        offset += 4

        name_len = struct.unpack_from(">I", data, offset)[0]
        offset += 4

        func_name = data[offset:offset + name_len].decode()
        offset += name_len

        arg_count = data[offset]
        offset += 1

        args = []

        for _ in range(arg_count):
            arg_type = data[offset]
            offset += 1

            if arg_type == 1:  # int
                val = struct.unpack_from(">I", data, offset)[0]
                offset += 4
                args.append(val)

            elif arg_type == 3:  # string
                strlen = struct.unpack_from(">I", data, offset)[0]
                offset += 4
                raw = data[offset:offset + strlen]
                offset += strlen
                args.append(raw.decode())

            else:
                args.append(f"<unknown type {arg_type}>")

        return {
            "packet_type": "FUNCTION",
            "client_id": client_id,
            "packet_id": packet_id,
            "function": func_name,
            "args": args,
        }

    except Exception as e:
        return {"error": f"Function parse error: {e}"}


# ------------------------------------------------------------
# Unified WonderlandX Packet Parser
# ------------------------------------------------------------
def parse_wonderland_packet(data: bytes):
    if not data:
        return {"error": "Empty packet"}

    if data[0] == ord('E'):
        return parse_event_packet(data)

    if data[0] == ord('F'):
        return parse_function_packet(data)

    if data[0] == ord('R'):
        return {"packet_type": "RETURN", "raw": data}

    return {"error": f"Unknown packet type {chr(data[0])}"}


# ------------------------------------------------------------
# Main Dev Tool
# ------------------------------------------------------------
def main():
    ctx = zmq.Context()
    sock = ctx.socket(zmq.DEALER)

    endpoint = "tcp://127.0.0.1:5555"
    print(f"[dev] Connecting to WonderlandX at {endpoint}")
    sock.connect(endpoint)

    sock.setsockopt(zmq.IDENTITY, b"dev-client")
    print("[dev] Waiting for WonderlandX messages...\n")

    while True:
        msg = sock.recv()

        print("=== WonderlandX Message Received ===")
        print(f"Raw bytes: {msg!r}")
        print(f"Hex dump:  {hex_dump(msg)}")

        parsed = parse_wonderland_packet(msg)

        print("\nParsed packet:")
        pprint(parsed)

        print("====================================\n")


if __name__ == "__main__":
    main()
