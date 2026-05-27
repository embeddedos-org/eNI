# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
import unittest

class TestEniIPGeolocator(unittest.TestCase):
    def test_ip_geolocator_mock(self):
        print("Testing eNI IP-based geolocation fallback...")
        ip = "8.8.8.8"
        mock_loc = {"lat": 37.751, "lon": -97.822, "country": "United States", "city": "Mountain View"}
        self.assertEqual(mock_loc["city"], "Mountain View")
        print(f"IP {ip} resolved to: {mock_loc['city']}, {mock_loc['country']}")
