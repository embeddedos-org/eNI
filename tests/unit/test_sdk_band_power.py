"""Regression tests for the Python SDK band-power binning (eNI issue #62).

`DSPPipeline.compute_band_power()` must put every PSD bin in at most one
band, using the half-open rule `low <= bin_centre_freq < high` — the same
rule `eni_dsp_band_power()` uses in C (`common/src/dsp.c`).

The SDK lives in `sdk/python/eni`, which collides by name with the root
`eni` package, so load the module straight from its file instead of
importing `eni.core`.
"""
import importlib.util
import os
import unittest

import numpy as np

_SDK_CORE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "sdk", "python", "eni", "core.py",
)


def _load_sdk_core():
    spec = importlib.util.spec_from_file_location("eni_sdk_core_for_test", _SDK_CORE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


sdk_core = _load_sdk_core()
DSPPipeline = sdk_core.DSPPipeline
EEG_BANDS = sdk_core.EEG_BANDS


class TestBandPowerBinning(unittest.TestCase):
    """Half-open [low, high) binning on the bin centre frequency."""

    def test_bin_on_shared_edge_belongs_to_upper_band_only(self):
        """freq_resolution 1.5 Hz: bin 5 (7.5 Hz) straddles the 8 Hz edge.

        It must land in theta only, not in both theta and alpha.
        """
        p = DSPPipeline(sample_rate=384, fft_size=256)  # df = 1.5 Hz
        psd = [0.0] * 128
        psd[5] = 1.0
        bands = p.compute_band_power(psd)

        self.assertAlmostEqual(bands["theta"], 1.5)  # 1.0 * df
        self.assertEqual(bands["alpha"], 0.0)
        self.assertAlmostEqual(sum(bands.values()), 1.5)

    def test_bin_exactly_on_an_edge_goes_to_the_upper_band(self):
        """1 Hz bins: bin 8 sits exactly on the 8 Hz theta/alpha edge.

        [low, high) is closed below, so it belongs to alpha.
        """
        p = DSPPipeline(sample_rate=256, fft_size=256)  # df = 1.0 Hz
        psd = [0.0] * 128
        psd[8] = 1.0
        bands = p.compute_band_power(psd)

        self.assertEqual(bands["theta"], 0.0)
        self.assertAlmostEqual(bands["alpha"], 1.0)

    def test_dc_bin_is_in_no_band(self):
        """Bin 0 is 0 Hz, below delta's 0.5 Hz lower edge."""
        p = DSPPipeline(sample_rate=256, fft_size=256)  # df = 1.0 Hz
        psd = [0.0] * 128
        psd[0] = 1.0
        bands = p.compute_band_power(psd)

        self.assertEqual(bands["delta"], 0.0)
        self.assertEqual(sum(bands.values()), 0.0)

    def test_every_bin_counted_at_most_once(self):
        """No bin may appear in two bands, for aligned or unaligned bins."""
        for sample_rate, fft_size in [(384, 256), (256, 256), (250, 256), (500, 512)]:
            with self.subTest(sample_rate=sample_rate, fft_size=fft_size):
                p = DSPPipeline(sample_rate=sample_rate, fft_size=fft_size)
                n_bins = fft_size // 2
                df = p.sample_rate / p.fft_size
                for idx in range(n_bins):
                    psd = [0.0] * n_bins
                    psd[idx] = 1.0
                    bands = p.compute_band_power(psd)
                    hits = [name for name, v in bands.items() if v != 0.0]
                    self.assertLessEqual(
                        len(hits), 1,
                        f"bin {idx} ({idx * df} Hz) counted in {hits}",
                    )

    def test_band_powers_sum_to_total_in_range_power(self):
        """Sum of the five bands == power over [0.5, 100) Hz, exactly.

        With freq_resolution 1.5 Hz none of 4/8/13 Hz is bin-aligned, which
        is the case that used to double-count.
        """
        sample_rate, fft_size = 384, 256
        p = DSPPipeline(sample_rate=sample_rate, fft_size=fft_size)
        df = sample_rate / fft_size

        t = np.arange(4096) / sample_rate
        signal = np.zeros(4096)
        for freq, amp in [(2.0, 3.0), (6.0, 2.0), (10.5, 4.0), (21.0, 1.5), (45.0, 1.0)]:
            signal += amp * np.sin(2.0 * np.pi * freq * t)

        psd = np.asarray(p.compute_psd(signal.tolist()), dtype=np.float64)
        bands = p.compute_band_power(psd.tolist())

        bin_freqs = np.arange(len(psd)) * df
        lo = min(low for low, _ in EEG_BANDS.values())
        hi = max(high for _, high in EEG_BANDS.values())
        total = float(psd[(bin_freqs >= lo) & (bin_freqs < hi)].sum() * df)

        self.assertAlmostEqual(sum(bands.values()), total, places=9)

    def test_matches_c_half_open_rule_bin_for_bin(self):
        """Replicates eni_dsp_band_power(): freq >= low_hz && freq < high_hz."""
        p = DSPPipeline(sample_rate=384, fft_size=256)
        df = p.sample_rate / p.fft_size
        rng = np.random.default_rng(62)
        psd = rng.random(128)

        bands = p.compute_band_power(psd.tolist())
        for name, (low, high) in EEG_BANDS.items():
            expected = 0.0
            for i in range(len(psd)):          # mirrors the C loop
                freq = i * df
                if freq >= low and freq < high:
                    expected += psd[i]
            self.assertAlmostEqual(bands[name], expected * df, places=12, msg=name)


if __name__ == "__main__":
    unittest.main()
