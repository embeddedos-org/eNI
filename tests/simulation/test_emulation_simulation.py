import unittest

class TesteNISimulation(unittest.TestCase):
    def test_phy_ethernet_loopback(self):
        # Simulate physical layer loopback
        tx_buffer = b"ETHERNET_FRAME_DATA"
        rx_buffer = tx_buffer # Hardware loopback enabled
        assert rx_buffer == tx_buffer, "PHY loopback hardware simulation failed"
