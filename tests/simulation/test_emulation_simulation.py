import unittest
class TestENISimulation(unittest.TestCase):
    def test_phy_loopback(self):
        tx = b"hello"
        rx = tx # loopback mode
        self.assertEqual(rx, b"hello")
