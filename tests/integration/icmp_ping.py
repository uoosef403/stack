from scapy.layers.inet import IP, ICMP
from scapy.sendrecv import sr1

def ping(host: str):
    pkt = IP(dst=host)/ICMP()
    reply = sr1(pkt, timeout=2, verbose=0)
    if reply:
        print(f"Reply from {host}: time={(reply.time - pkt.sent_time)*1000:.2f} ms")
    else:
        print(f"Request timed out for {host}")

if __name__ == "__main__":
    ping("192.168.69.69")
