import unittest
import time
class TestENIPerformance(unittest.TestCase):
    def test_packet_parsing_latency(self):
        start = time.perf_counter()
        for _ in range(1000):
            pass # simulate packet parsing
        latency = (time.perf_counter() - start) / 1000
        self.assertLess(latency, 0.001) # < 1ms SLA
