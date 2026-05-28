import unittest
from eni.geolocation.ip_geolocator import IPGeolocator


class TestIPGeolocator(unittest.TestCase):
    """IP-based geolocation fallback — validates contract, not a hardcoded city."""

    def test_ip_geolocation_status(self):
        geolocator = IPGeolocator()
        res = geolocator.get_location_by_ip()
        assert res["status"] in ["success", "fallback"]

    def test_ip_geolocation_city_is_string(self):
        geolocator = IPGeolocator()
        res = geolocator.get_location_by_ip()
        assert isinstance(res["city"], str) and len(res["city"]) > 0

    def test_ip_geolocation_coordinates_in_range(self):
        geolocator = IPGeolocator()
        res = geolocator.get_location_by_ip()
        assert -90.0 <= res["latitude"] <= 90.0
        assert -180.0 <= res["longitude"] <= 180.0
