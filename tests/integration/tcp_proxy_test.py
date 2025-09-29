import random
import time
import threading
from scapy.all import sniff
from scapy.layers.inet import IP, TCP
from scapy.sendrecv import send

# --- Test Configuration ---
# The IP of your TUN interface that the stack is running on.
STACK_IP = "192.168.69.69" 
# The REAL destination IP you want to connect to *through* the proxy.
# Using a reliable public DNS server is a good choice.
REAL_DEST_IP = "9.9.9.9" 
DEST_PORT = 80  # A common port like HTTP is best.
SRC_PORT = random.randint(1024, 65535)

# We need a shared state for our threads
class ConnectionState:
    def __init__(self):
        self.our_seq = random.randint(1000, 2000)
        self.their_seq = 0
        self.handshake_complete = threading.Event()
        self.data_received = threading.Event()
        self.fin_received = threading.Event()
        self.received_payload = b""

state = ConnectionState()

def packet_callback(pkt):
    """
    This function is called by the sniffer for every packet it sees.
    It processes replies from the stack and updates our connection state.
    """
    if not pkt.haslayer(TCP):
        return

    # Ensure the packet is for our connection
    if pkt[TCP].sport != DEST_PORT or pkt[TCP].dport != SRC_PORT:
        return

    print(f"   [<--] Received packet with flags: {pkt[TCP].flags}")

    # Handle SYN/ACK during handshake
    if pkt[TCP].flags == 'SA':
        state.their_seq = pkt[TCP].seq + 1
        state.handshake_complete.set() # Signal the main thread to continue
        return

    # Handle incoming data from the real server (forwarded by the proxy)
    if pkt.haslayer(TCP) and pkt[TCP].payload:
        payload = bytes(pkt[TCP].payload)
        print(f"   [<--] Received {len(payload)} bytes of data.")
        state.received_payload += payload
        state.their_seq = pkt[TCP].seq + len(payload)
        state.data_received.set()

    # Handle the stack's FIN packet during teardown
    if 'F' in pkt[TCP].flags:
        state.their_seq = pkt[TCP].seq + 1
        state.fin_received.set()


def run_proxy_test():
    """Main test function to orchestrate the TCP proxy test."""
    
    # The IP layer is always from our script to the STACK_IP,
    # but the TCP layer is addressed to the REAL destination.
    ip_layer = IP(dst=STACK_IP, src=REAL_DEST_IP) # Source is spoofed to match return packets

    # --- 1. Handshake ---
    print("--- 1. Testing Handshake ---")
    print("   [-->] Sending SYN...")
    syn_pkt = TCP(sport=SRC_PORT, dport=DEST_PORT, flags='S', seq=state.our_seq)
    send(ip_layer/syn_pkt, verbose=0)
    
    # Wait for the SYN/ACK to be received by our sniffer (max 5 seconds)
    if not state.handshake_complete.wait(timeout=5):
        print("   [!] FAILED: Handshake timed out. Did not receive SYN/ACK.")
        return False

    state.our_seq += 1
    print("   [-->] Sending ACK to complete handshake...")
    ack_pkt = TCP(sport=SRC_PORT, dport=DEST_PORT, flags='A', seq=state.our_seq, ack=state.their_seq)
    send(ip_layer/ack_pkt, verbose=0)
    print("   [+] PASSED: Handshake successful.")
    
    # --- 2. Data Transfer (HTTP GET) ---
    print("\n--- 2. Testing Data Transfer ---")
    http_get = b"GET / HTTP/1.0\r\nHost: " + REAL_DEST_IP.encode() + b"\r\n\r\n"
    print(f"   [-->] Sending HTTP GET request ({len(http_get)} bytes)...")
    data_pkt = TCP(sport=SRC_PORT, dport=DEST_PORT, flags='PA', seq=state.our_seq, ack=state.their_seq)
    send(ip_layer/data_pkt/http_get, verbose=0)
    state.our_seq += len(http_get)

    # Wait for some data to come back from the real server
    if not state.data_received.wait(timeout=5):
        print("   [!] FAILED: Data transfer timed out. Did not receive any response.")
        return False
    
    # Acknowledge the data we received
    print(f"   [-->] Sending ACK for received data...")
    ack_data_pkt = TCP(sport=SRC_PORT, dport=DEST_PORT, flags='A', seq=state.our_seq, ack=state.their_seq)
    send(ip_layer/ack_data_pkt, verbose=0)

    # Validate the received data
    if b"HTTP" in state.received_payload:
        print("   [+] PASSED: Received a valid HTTP response.")
    else:
        print("   [!] FAILED: Did not receive a valid HTTP response.")
        return False

    # --- 3. Teardown ---
    print("\n--- 3. Testing Teardown ---")
    print("   [-->] Sending FIN...")
    fin_pkt = TCP(sport=SRC_PORT, dport=DEST_PORT, flags='FA', seq=state.our_seq, ack=state.their_seq)
    send(ip_layer/fin_pkt, verbose=0)
    state.our_seq += 1

    # Wait for the stack to send its FIN
    if not state.fin_received.wait(timeout=5):
        print("   [!] FAILED: Teardown timed out. Did not receive FIN from stack.")
        return False

    print("   [-->] Sending final ACK...")
    final_ack_pkt = TCP(sport=SRC_PORT, dport=DEST_PORT, flags='A', seq=state.our_seq, ack=state.their_seq)
    send(ip_layer/final_ack_pkt, verbose=0)
    print("   [+] PASSED: Teardown successful.")
    return True


if __name__ == "__main__":
    # The interface name for your TUN device (e.g., 'tun0', 'Ethernet 2')
    # You may need to change this! Find it with `ipconfig` on Windows or `ip a` on Linux.
    IFACE = "tun0" 
    
    # Setup the sniffer to run in a background thread
    # It filters for packets coming FROM the stack TO us
    filter_str = f"tcp and dst host {REAL_DEST_IP} and dst port {SRC_PORT}"
    sniffer = threading.Thread(
        target=lambda: sniff(iface=IFACE, filter=filter_str, prn=packet_callback, stop_filter=lambda p: state.fin_received.is_set()),
        daemon=True
    )

    print(f"Starting TCP proxy test for destination {REAL_DEST_IP}:{DEST_PORT}")
    print(f"Sniffing on interface '{IFACE}'...\n")
    
    sniffer.start()
    time.sleep(1) # Give the sniffer a moment to start up

    success = run_proxy_test()

    print("\n-------------------------")
    if success:
        print("✅✅✅ ALL TESTS PASSED ✅✅✅")
    else:
        print("❌❌❌ TEST SUITE FAILED ❌❌❌")
    print("-------------------------")