import unittest
from eni.geolocation.ip_geolocator import IPGeolocator

class TestIPGeolocator(unittest.TestCase):
    def test_ip_geolocation(self):
        geolocator = IPGeolocator()
        res = geolocator.get_location_by_ip()
        assert res["status"] in ["success", "fallback"]
        assert res["city"] == "San Francisco"
