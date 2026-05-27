import unittest

class TesteNIPerformance(unittest.TestCase):
    import time
    def test_packet_parsing_latency(self):
        packet_raw = b"\x45\x00\x00\x28\x00\x01\x00\x00\x40\x06\x7c\xcd\x7f\x00\x00\x01\x7f\x00\x00\x01"
        start = time.perf_counter_ns()
        # Parse IPv4 header (version, IHL, total length)
        version = (packet_raw[0] >> 4) & 0xF
        ihl = packet_raw[0] & 0xF
        total_len = (packet_raw[2] << 8) | packet_raw[3]
        end = time.perf_counter_ns()
        assert version == 4
        assert ihl == 5
        assert total_len == 40
        assert (end - start) < 1000, "Packet parsing took longer than 1µs SLA"
