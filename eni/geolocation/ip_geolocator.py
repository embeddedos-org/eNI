
import urllib.request
import json

class IPGeolocator:
    def __init__(self):
        self.api_url = "https://ipapi.co/json/"
        
    def get_location_by_ip(self):
        """
        Get the device's physical location based on external IP using ipapi.co.
        Used as a high-reliability fallback when GPS satellite lock is unavailable.
        """
        req = urllib.request.Request(
            self.api_url,
            headers={"User-Agent": "eNI-Network-Stack/1.0"}
        )
        try:
            with urllib.request.urlopen(req, timeout=5) as response:
                data = json.loads(response.read().decode())
                return {
                    "latitude": data.get("latitude", 37.7749),
                    "longitude": data.get("longitude", -122.4194),
                    "city": data.get("city", "San Francisco"),
                    "country": data.get("country_name", "United States"),
                    "status": "success"
                }
        except Exception as e:
            return {
                "latitude": 37.7749,
                "longitude": -122.4194,
                "city": "San Francisco",
                "country": "United States",
                "status": "fallback",
                "error": str(e)
            }
