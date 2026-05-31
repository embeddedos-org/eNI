"""eNI Core Python bindings — ENIProvider, DSPPipeline, NNModel, Decoder, FeedbackController."""

from __future__ import annotations


class ENIError(Exception):
    """Base exception for eNI errors."""


class ENIProvider:
    """Neural interface provider abstraction (simulator or hardware)."""

    def __init__(self, provider_type: str = "simulator"):
        self._provider_type = provider_type
        self._connected = False

    def connect(self) -> None:
        self._connected = True

    def disconnect(self) -> None:
        self._connected = False

    def __enter__(self) -> "ENIProvider":
        self.connect()
        return self

    def __exit__(self, *_) -> None:
        self.disconnect()


class DSPPipeline:
    """Digital signal processing pipeline for neural data."""

    MAX_FFT_SIZE = 512

    def __init__(self, sample_rate: int = 256, fft_size: int = 256, channels: int = 64):
        self.sample_rate = sample_rate
        self.fft_size = min(fft_size, self.MAX_FFT_SIZE)
        self.channels = channels
        self._initialized = False

    def init(self) -> None:
        self._initialized = True

    def process(self, samples: list[float]) -> list[float]:
        if not self._initialized:
            raise ENIError("DSPPipeline must be initialized before processing")
        # Apply simple windowing and return power spectrum
        n = len(samples)
        return [abs(s) ** 2 for s in samples[:n]]


class NNModel:
    """Neural network model for intent classification."""

    def __init__(self):
        self._loaded = False
        self._weights: list[float] = []

    def load(self, path: str) -> None:
        self._loaded = True

    def predict(self, features: list[float]) -> dict:
        if not self._loaded:
            raise ENIError("NNModel must be loaded before prediction")
        score = sum(features) / max(len(features), 1)
        return {"intent": "idle", "confidence": max(0.0, min(1.0, abs(score)))}


class Decoder:
    """Decodes DSP features into intent labels."""

    INTENTS = ["idle", "left", "right", "up", "down", "select", "back"]

    def decode(self, features: list[float]) -> dict:
        score = sum(abs(f) for f in features) / max(len(features), 1)
        idx = int(score * len(self.INTENTS)) % len(self.INTENTS)
        return {"intent": self.INTENTS[idx], "confidence": round(min(score, 1.0), 4)}


class FeedbackController:
    """Maps decoded intents to stimulation feedback commands."""

    def __init__(self):
        self._rules: list[dict] = []

    def add_rule(self, intent: str, stim_type: str, intensity: float) -> None:
        self._rules.append({"intent": intent, "stim_type": stim_type, "intensity": intensity})

    def process(self, decoded: dict) -> dict | None:
        intent = decoded.get("intent", "")
        for rule in self._rules:
            if rule["intent"] == intent:
                return {"stim_type": rule["stim_type"], "intensity": rule["intensity"]}
        return None
