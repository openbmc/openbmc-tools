# This script is used for printing out the number of packets in a pcap file

from scapy.all import rdpcap
import sys

file_name = sys.argv[1]
try:
    stream = rdpcap(file_name)
    n = 0
    for packet in stream:
        n += 1
    print(n)
except Exception as e:
    pass
