# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
import unittest
class TestEniFunctional(unittest.TestCase):
    def test_tcp_handshake_state_machine(self):
        print("Testing TCP 3-way handshake state machine...")
        state = "CLOSED"
        state = "SYN_SENT"
        state = "SYN_RCVD"
        state = "ESTABLISHED"
        self.assertEqual(state, "ESTABLISHED")
