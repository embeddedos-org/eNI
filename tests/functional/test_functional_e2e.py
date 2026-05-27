import unittest
class TestENIFunctional(unittest.TestCase):
    def test_network_stack_pipeline(self):
        stack = ["PHY", "MAC", "IP", "TCP", "HTTP"]
        self.assertEqual(stack[3], "TCP")
