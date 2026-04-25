"""eNI Core — ctypes bindings for libeni_common."""

import ctypes
import ctypes.util
import os
import platform
import time
from pathlib import Path

import numpy as np


def _find_library():
    """Locate the eNI shared library."""
    search_paths = [
        Path(__file__).parent.parent.parent.parent / "build" / "common",
        Path(__file__).parent.parent.parent.parent / "build" / "release" / "common",
        Path("/usr/local/lib"),
        Path("/usr/lib"),
    ]

    lib_name = {
        "Linux": "libeni_common.so",
        "Darwin": "libeni_common.dylib",
        "Windows": "eni_common.dll",
    }.get(platform.system(), "libeni_common.so")

    for p in search_paths:
        lib_path = p / lib_name
        if lib_path.exists():
            return str(lib_path)

    found = ctypes.util.find_library("eni_common")
    if found:
        return found

    return None


class ENIError(Exception):
    """eNI library error."""
    pass


ENI_OK = 0
ENI_MAX_CHANNELS = 1024
ENI_DSP_MAX_FFT_SIZE = 512

# EEG frequency band definitions (Hz)
EEG_BANDS = {
    "delta": (0.5, 4.0),
    "theta": (4.0, 8.0),
    "alpha": (8.0, 13.0),
    "beta": (13.0, 30.0),
    "gamma": (30.0, 100.0),
}


class ENIProvider:
    """RAII wrapper for an eNI neural input provider."""

    def __init__(self, provider_type="simulator"):
        self._lib = None
        self._provider_type = provider_type
        self._connected = False
        self._num_channels = 64
        self._sample_rate = 256

        # Try to load the native library
        try:
            self._lib = self._load_lib()
        except ENIError:
            self._lib = None  # Fall back to synthetic data

    @staticmethod
    def _load_lib():
        path = _find_library()
        if path is None:
            raise ENIError("Cannot find libeni_common. Build the project first.")
        return ctypes.CDLL(path)

    def connect(self):
        """Connect to the provider."""
        self._connected = True
        return self

    def disconnect(self):
        """Disconnect from the provider."""
        self._connected = False

    def poll(self):
        """Poll for a neural data packet.

        If the native C library is available, reads samples via ctypes.
        Otherwise, generates synthetic test data (sine waves at known
        frequencies across channels) for development/testing.
        """
        if not self._connected:
            raise ENIError("Provider not connected")

        timestamp = time.time()

        if self._lib is not None:
            # Read from native C library via ctypes
            try:
                c_samples = (ctypes.c_float * self._num_channels)()
                c_num_ch = ctypes.c_uint32(0)
                c_ts = ctypes.c_uint64(0)

                ret = self._lib.eni_provider_poll(
                    ctypes.byref(c_samples),
                    ctypes.byref(c_num_ch),
                    ctypes.byref(c_ts),
                )
                if ret == ENI_OK:
                    n = min(c_num_ch.value, self._num_channels)
                    samples = [c_samples[i] for i in range(n)]
                    return {
                        "samples": samples,
                        "num_channels": n,
                        "timestamp": c_ts.value,
                    }
            except (OSError, AttributeError):
                pass  # Fall through to synthetic data

        # Generate synthetic test data: sum of sine waves at EEG band
        # center frequencies with decreasing amplitude per channel.
        t = timestamp
        samples = []
        for ch in range(self._num_channels):
            # Each channel gets a slightly different phase and mix
            phase = ch * 0.1
            val = (
                10.0 * np.sin(2.0 * np.pi * 2.0 * t + phase)     # delta ~2Hz
                + 5.0 * np.sin(2.0 * np.pi * 6.0 * t + phase)    # theta ~6Hz
                + 8.0 * np.sin(2.0 * np.pi * 10.0 * t + phase)   # alpha ~10Hz
                + 3.0 * np.sin(2.0 * np.pi * 20.0 * t + phase)   # beta ~20Hz
                + 1.0 * np.sin(2.0 * np.pi * 40.0 * t + phase)   # gamma ~40Hz
                + np.random.normal(0, 0.5)                         # noise
            )
            samples.append(float(val))

        return {
            "samples": samples,
            "num_channels": self._num_channels,
            "timestamp": int(timestamp * 1000),
        }

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.disconnect()

    def __del__(self):
        if self._connected:
            self.disconnect()


class DSPPipeline:
    """Digital Signal Processing pipeline."""

    def __init__(self, sample_rate=256, fft_size=256, channels=64):
        self.sample_rate = sample_rate
        self.fft_size = min(fft_size, ENI_DSP_MAX_FFT_SIZE)
        self.channels = channels
        self._initialized = False

    def init(self):
        """Initialize the DSP engine."""
        self._initialized = True
        return self

    def process(self, samples):
        """Process a block of samples through the DSP pipeline."""
        if not self._initialized:
            raise ENIError("DSP pipeline not initialized")
        psd = self.compute_psd(samples)
        band_power = self.compute_band_power(psd)
        features = samples[: self.channels] if len(samples) >= self.channels else samples
        return {
            "psd": psd,
            "band_power": band_power,
            "features": features,
        }

    def compute_psd(self, samples):
        """Compute power spectral density using Welch's method.

        The signal is divided into overlapping segments, each windowed
        with a Hanning window, FFT'd, and the resulting power spectra
        are averaged.

        Args:
            samples: 1-D array-like of time-domain samples.

        Returns:
            List of PSD values (length = fft_size // 2) in units of V^2/Hz.
        """
        data = np.asarray(samples, dtype=np.float64)
        n = len(data)
        seg_len = self.fft_size
        overlap = seg_len // 2  # 50% overlap

        if n < seg_len:
            # Pad with zeros if signal is shorter than one segment
            data = np.pad(data, (0, seg_len - n), mode="constant")
            n = seg_len

        # Hanning window and its power for normalization
        window = np.hanning(seg_len)
        win_power = np.sum(window ** 2)

        # Segment the signal with 50% overlap
        step = seg_len - overlap
        num_segments = max(1, (n - seg_len) // step + 1)

        psd_accum = np.zeros(seg_len // 2)

        for i in range(num_segments):
            start = i * step
            segment = data[start : start + seg_len]
            if len(segment) < seg_len:
                segment = np.pad(segment, (0, seg_len - len(segment)), mode="constant")

            # Apply window
            windowed = segment * window

            # FFT and take one-sided power spectrum
            fft_result = np.fft.rfft(windowed)
            power = np.abs(fft_result[:seg_len // 2]) ** 2

            # Normalize: divide by (sample_rate * window_power)
            power = power / (self.sample_rate * win_power)

            # Double the non-DC, non-Nyquist bins for one-sided spectrum
            power[1:] *= 2.0

            psd_accum += power

        # Average across segments
        psd_accum /= num_segments

        return psd_accum.tolist()

    def compute_band_power(self, psd):
        """Compute band power by integrating PSD over standard EEG bands.

        Uses the trapezoidal approximation: power = sum(PSD[f1:f2]) * df
        where df = sample_rate / fft_size.

        Args:
            psd: Power spectral density array (from compute_psd).

        Returns:
            Dict with keys: delta, theta, alpha, beta, gamma (float values).
        """
        psd_arr = np.asarray(psd, dtype=np.float64)
        n_bins = len(psd_arr)
        freq_resolution = self.sample_rate / self.fft_size

        result = {}
        for band_name, (f_low, f_high) in EEG_BANDS.items():
            # Convert frequency limits to bin indices
            idx_low = max(0, int(np.floor(f_low / freq_resolution)))
            idx_high = min(n_bins, int(np.ceil(f_high / freq_resolution)))

            if idx_low >= idx_high or idx_low >= n_bins:
                result[band_name] = 0.0
            else:
                # Integrate PSD over the band (trapezoidal approximation)
                result[band_name] = float(
                    np.sum(psd_arr[idx_low:idx_high]) * freq_resolution
                )

        return result


class NNModel:
    """Neural network model for inference using ONNX Runtime."""

    def __init__(self, model_path=None):
        self.model_path = model_path
        self._loaded = False
        self.num_classes = 0
        self._session = None
        self._input_name = None
        self._output_name = None

    def load(self, path=None):
        """Load an ONNX model from file.

        Args:
            path: Path to .onnx model file. Falls back to self.model_path.

        Returns:
            self (for chaining).
        """
        p = path or self.model_path
        if not p or not os.path.exists(p):
            raise ENIError(f"Model file not found: {p}")

        try:
            import onnxruntime as ort

            self._session = ort.InferenceSession(
                p, providers=["CPUExecutionProvider"]
            )
            inputs = self._session.get_inputs()
            outputs = self._session.get_outputs()

            if not inputs or not outputs:
                raise ENIError("Model has no inputs or outputs")

            self._input_name = inputs[0].name
            self._output_name = outputs[0].name

            # Infer num_classes from output shape
            out_shape = outputs[0].shape
            self.num_classes = out_shape[-1] if len(out_shape) > 1 else out_shape[0]
            self._loaded = True

        except ImportError:
            raise ENIError(
                "onnxruntime is required for model inference. "
                "Install with: pip install onnxruntime"
            )

        return self

    def predict(self, features):
        """Run inference on a feature vector.

        Args:
            features: 1-D array-like of input features.

        Returns:
            Dict with:
                class_id: int — predicted class index (argmax)
                confidence: float — probability of predicted class
                probabilities: list[float] — per-class probabilities
        """
        if not self._loaded or self._session is None:
            raise ENIError("Model not loaded")

        input_arr = np.asarray(features, dtype=np.float32)

        # Reshape to batch dimension: (1, num_features)
        if input_arr.ndim == 1:
            input_arr = input_arr.reshape(1, -1)

        outputs = self._session.run(
            [self._output_name],
            {self._input_name: input_arr},
        )

        logits = outputs[0].flatten()

        # Apply softmax to get probabilities
        exp_logits = np.exp(logits - np.max(logits))
        probabilities = exp_logits / np.sum(exp_logits)

        class_id = int(np.argmax(probabilities))
        confidence = float(probabilities[class_id])

        return {
            "class_id": class_id,
            "confidence": confidence,
            "probabilities": probabilities.tolist(),
        }


class Decoder:
    """Intent decoder combining DSP and NN inference."""

    # Default intent labels for BCI applications
    DEFAULT_LABELS = [
        "idle",
        "left_hand",
        "right_hand",
        "feet",
        "tongue",
        "focus",
        "relax",
    ]

    def __init__(self, decoder_type="energy", labels=None):
        self.decoder_type = decoder_type
        self._dsp = DSPPipeline()
        self._model = NNModel()
        self._labels = labels or self.DEFAULT_LABELS

    def decode(self, samples):
        """Decode neural intent from raw samples.

        Pipeline:
            1. Compute PSD via Welch's method
            2. Extract band power features
            3. Run NN model inference (if loaded)
            4. Map output index to intent label with confidence

        Args:
            samples: 1-D array-like of raw EEG samples.

        Returns:
            Dict with:
                intent: str — decoded intent label
                confidence: float — confidence score [0, 1]
        """
        if not self._dsp._initialized:
            self._dsp.init()

        # Step 1-2: DSP feature extraction
        psd = self._dsp.compute_psd(samples)
        band_power = self._dsp.compute_band_power(psd)

        # Build feature vector from band powers
        features = [
            band_power.get("delta", 0.0),
            band_power.get("theta", 0.0),
            band_power.get("alpha", 0.0),
            band_power.get("beta", 0.0),
            band_power.get("gamma", 0.0),
        ]

        # Step 3: Model inference if available
        if self._model._loaded:
            try:
                result = self._model.predict(features)
                class_id = result["class_id"]
                confidence = result["confidence"]

                # Map to label
                if class_id < len(self._labels):
                    intent = self._labels[class_id]
                else:
                    intent = f"class_{class_id}"

                return {"intent": intent, "confidence": confidence}
            except Exception:
                pass  # Fall through to energy-based decoding

        # Fallback: energy-based decoding (no model needed)
        # Use relative alpha power as a simple focus/relax detector
        total_power = sum(features) or 1.0
        alpha_ratio = features[2] / total_power  # alpha band
        beta_ratio = features[3] / total_power   # beta band

        if alpha_ratio > 0.4:
            return {"intent": "relax", "confidence": float(alpha_ratio)}
        elif beta_ratio > 0.3:
            return {"intent": "focus", "confidence": float(beta_ratio)}
        else:
            return {"intent": "idle", "confidence": float(1.0 - alpha_ratio - beta_ratio)}


class FeedbackController:
    """Feedback controller for stimulation."""

    def __init__(self):
        self._active = False
        self._rules = []

    def add_rule(self, intent, stim_type, amplitude):
        """Add a feedback rule."""
        self._rules.append({"intent": intent, "stim_type": stim_type, "amplitude": amplitude})

    def process(self, decoded_intent):
        """Process decoded intent and generate feedback."""
        for rule in self._rules:
            if rule["intent"] == decoded_intent.get("intent"):
                return {"stim_type": rule["stim_type"], "amplitude": rule["amplitude"]}
        return None

    def start(self):
        self._active = True

    def stop(self):
        self._active = False
