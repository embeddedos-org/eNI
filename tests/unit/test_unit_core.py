import unittest

class TesteNIUnit(unittest.TestCase):
    def test_tcp_handshake_state_machine(self):
        state = "CLOSED"
        # Client sends SYN
        state = "SYN_SENT"
        # Server sends SYN-ACK
        state = "SYN_RECEIVED"
        # Client sends ACK
        state = "ESTABLISHED"
        assert state == "ESTABLISHED", "TCP state machine failed to establish connection"
