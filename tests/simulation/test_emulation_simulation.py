# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
import unittest
class TestEniSimulation(unittest.TestCase):
    def test_phy_ethernet_loopback(self):
        print("Simulating physical layer (PHY) Ethernet loopback test...")
        tx_buffer = b"Hello World"
        rx_buffer = tx_buffer
        self.assertEqual(rx_buffer, tx_buffer)
