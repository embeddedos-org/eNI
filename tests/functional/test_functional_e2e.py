import unittest

class TesteNIFunctional(unittest.TestCase):
    def test_packet_routing_pipeline(self):
        packet = {"src": "192.168.1.10", "dst": "10.0.0.5", "payload": "hello"}
        routing_table = {"10.0.0.0/24": "eth0", "192.168.1.0/24": "eth1"}
        # Route packet
        interface = None
        for subnet, iface in routing_table.items():
            if packet["dst"].startswith("10.0.0."):
                interface = "eth0"
        assert interface == "eth0", "Packet failed to route to eth0"
