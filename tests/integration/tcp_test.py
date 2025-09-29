import random
from scapy.layers.inet import IP, TCP
from scapy.sendrecv import sr1, send

# --- Test Configuration ---
HOST = "192.168.69.69"  # The IP address of your TUN interface
DST_PORT = 80  # The port you are testing
SRC_PORT = random.randint(1024, 65535)  # Use a random source port


def test_handshake():
    """Tests the TCP Three-Way Handshake (SYN -> SYN/ACK -> ACK)"""
    print("--- 1. Testing Handshake ---")

    # 1. Send SYN
    print("   [S] Sending SYN...")
    ip = IP(dst=HOST)
    syn = TCP(sport=SRC_PORT, dport=DST_PORT, flags='S', seq=1000)
    syn_ack_reply = sr1(ip / syn, timeout=2, verbose=0)

    if not syn_ack_reply:
        print("   [!] FAILED: No SYN/ACK received.")
        return None

    # 2. Validate SYN/ACK
    if not (syn_ack_reply.haslayer(TCP) and syn_ack_reply[TCP].flags == 'SA'):
        print("   [!] FAILED: Reply was not a SYN/ACK.")
        return None

    print("   [SA] Received SYN/ACK. Correct.")

    # 3. Send final ACK to establish connection
    print("   [A] Sending ACK...")
    ack_seq = syn_ack_reply[TCP].seq + 1
    our_seq = syn[TCP].seq + 1

    ack = TCP(sport=SRC_PORT, dport=DST_PORT, flags='A', seq=our_seq, ack=ack_seq)
    send(ip / ack, verbose=0)

    print("   [+] PASSED: Handshake complete.")
    # Return the sequence numbers needed for the next test
    return {"ip": ip, "seq": our_seq, "ack": ack_seq}


def test_data_transfer(conn_details):
    """Tests sending data and receiving an ACK"""
    if not conn_details:
        return None

    print("\n--- 2. Testing Data Transfer ---")
    ip = conn_details["ip"]
    payload = "hello stack"

    # 1. Send data packet (PSH/ACK)
    print(f"   [PA] Sending data: '{payload}'")
    data_packet = TCP(
        sport=SRC_PORT,
        dport=DST_PORT,
        flags='PA',
        seq=conn_details["seq"],
        ack=conn_details["ack"]
    )

    ack_reply = sr1(ip / data_packet / payload, timeout=2, verbose=0)

    if not ack_reply:
        print("   [!] FAILED: Did not receive ACK for data.")
        return None

    # 2. Validate the ACK
    expected_ack = conn_details["seq"] + len(payload)
    if not (ack_reply.haslayer(TCP) and ack_reply[TCP].flags == 'A' and ack_reply[TCP].ack == expected_ack):
        print(f"   [!] FAILED: Invalid ACK received. Expected ACK={expected_ack}, Got ACK={ack_reply[TCP].ack}")
        return None

    print("   [A] Received correct ACK for data. Correct.")
    print("   [+] PASSED: Data transfer successful.")
    # Return updated sequence numbers
    conn_details["seq"] += len(payload)
    return conn_details


def test_teardown(conn_details):
    """Tests the graceful connection teardown (FIN -> ACK -> FIN/ACK)"""
    if not conn_details:
        return

    print("\n--- 3. Testing Connection Teardown ---")
    ip = conn_details["ip"]

    # 1. Send FIN/ACK
    print("   [FA] Sending FIN...")
    fin_packet = TCP(
        sport=SRC_PORT,
        dport=DST_PORT,
        flags='FA',
        seq=conn_details["seq"],
        ack=conn_details["ack"]
    )

    # The first reply should be an ACK for our FIN
    ack_reply = sr1(ip / fin_packet, timeout=2, verbose=0)

    if not ack_reply:
        print("   [!] FAILED: Did not receive an ACK for our FIN.")
        return

    print("   [A] Received ACK for our FIN. Correct.")
    print("   [*] The stack should now send its own FIN/ACK. Check logs/Wireshark.")
    print("   [+] PASSED: Teardown initiated correctly.")


if __name__ == "__main__":
    print(f"Starting TCP test suite for {HOST}:{DST_PORT}\n")

    # Run the tests in sequence
    connection = test_handshake()
    if connection:
        connection = test_data_transfer(connection)
    if connection:
        test_teardown(connection)

    print("\nTest suite finished.")
