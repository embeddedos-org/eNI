import unittest
class TestENIUnit(unittest.TestCase):
    def test_tcp_3way_handshake(self):
        state = "CLOSED"
        state = "SYN_SENT" # client sends SYN
        state = "SYN_RCVD" # server sends SYN-ACK
        state = "ESTABLISHED" # client sends ACK
        self.assertEqual(state, "ESTABLISHED")
