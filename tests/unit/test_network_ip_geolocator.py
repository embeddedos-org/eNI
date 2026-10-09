"""Unit tests for network/ip_geolocator.py — the ip-api.com location fallback.

Offline: requests.get is mocked, so every path (success, API-level
error, HTTP error, transport failure) runs deterministically in CI.
Part of the eNI#37 coverage drive — this module previously had no tests.
"""

import unittest
from unittest import mock

import pytest

requests = pytest.importorskip("requests")

from network.ip_geolocator import EniIPGeolocator


def _response(status_code=200, payload=None):
    resp = mock.MagicMock()
    resp.status_code = status_code
    resp.json.return_value = payload if payload is not None else {}
    return resp


_OK_PAYLOAD = {
    "status": "success",
    "lat": 48.85,
    "lon": 2.35,
    "country": "France",
    "city": "Paris",
    "isp": "Example ISP",
    "timezone": "Europe/Paris",
}


class TestEniIPGeolocator(unittest.TestCase):
    def test_success_maps_fields(self):
        with mock.patch.object(requests, "get", return_value=_response(payload=_OK_PAYLOAD)) as get:
            res = EniIPGeolocator().get_location_by_ip("1.2.3.4")
        self.assertEqual(
            res,
            {
                "lat": 48.85,
                "lon": 2.35,
                "country": "France",
                "city": "Paris",
                "isp": "Example ISP",
                "timezone": "Europe/Paris",
            },
        )
        url = get.call_args.args[0]
        self.assertEqual(url, "http://ip-api.com/json/1.2.3.4")
        self.assertEqual(get.call_args.kwargs["timeout"], 5)

    def test_empty_ip_queries_the_base_url(self):
        with mock.patch.object(requests, "get", return_value=_response(payload=_OK_PAYLOAD)) as get:
            EniIPGeolocator().get_location_by_ip()
        self.assertEqual(get.call_args.args[0], "http://ip-api.com/json/")

    def test_api_failure_status_returns_labelled_error(self):
        payload = {"status": "fail", "message": "invalid query"}
        with mock.patch.object(requests, "get", return_value=_response(payload=payload)):
            res = EniIPGeolocator().get_location_by_ip("bad")
        self.assertEqual(res, {"error": "invalid query"})

    def test_http_error_returns_labelled_error(self):
        with mock.patch.object(requests, "get", return_value=_response(status_code=429)):
            res = EniIPGeolocator().get_location_by_ip()
        self.assertEqual(res, {"error": "HTTP 429"})

    def test_transport_failure_returns_labelled_error(self):
        with mock.patch.object(requests, "get", side_effect=requests.Timeout("timed out")):
            res = EniIPGeolocator().get_location_by_ip()
        self.assertEqual(res, {"error": "timed out"})


if __name__ == "__main__":
    unittest.main()
