import requests

class EniIPGeolocator:
    """
    IP-based Geolocation fallback API client for network interfaces
    when physical GPS lock is lost.
    """
    def __init__(self):
        # Uses ip-api.com (reliable, public, free IP geolocation API)
        self.base_url = "http://ip-api.com/json/"

    def get_location_by_ip(self, ip_address: str = "") -> dict:
        """
        Fetch latitude, longitude, country, and city by IP address.
        """
        url = f"{self.base_url}{ip_address}"
        try:
            response = requests.get(url, timeout=5)
            if response.status_code == 200:
                data = response.json()
                if data.get("status") == "success":
                    return {
                        "lat": data.get("lat"),
                        "lon": data.get("lon"),
                        "country": data.get("country"),
                        "city": data.get("city"),
                        "isp": data.get("isp"),
                        "timezone": data.get("timezone")
                    }
                return {"error": data.get("message", "Unknown error")}
            return {"error": f"HTTP {response.status_code}"}
        except Exception as e:
            return {"error": str(e)}
