# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project

"""Unit tests for the uncovered validation paths in tools/quantize_model.py.

Covers read_eni_model's version/layer-count/weight-count guards, the
already-quantized refusal, the --info ONNX path without the onnx package,
and the _output_valid fail-closed gates (zero-weight and bad-dequant
outputs are deleted, never written). Part of the eNI#44 coverage drive.
"""

import os
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.quantize_model import (
    MODEL_MAGIC,
    VERSION_FLOAT,
    VERSION_QUANTIZED,
    main,
    read_eni_model,
    write_eni_model,
)

try:
    import onnx  # noqa: F401

    HAS_ONNX = True
except ImportError:
    HAS_ONNX = False


def _header(version, n_layers):
    return struct.pack("<III", MODEL_MAGIC, version, n_layers)


def _layer_header(name, input_size, output_size, activation=1):
    name_b = name.encode("utf-8")[:32].ljust(32, b"\x00")
    return name_b + struct.pack("<III", input_size, output_size, activation)


class TestReadValidation(unittest.TestCase):
    def _write(self, d, blob):
        p = os.path.join(d, "m.eni_model")
        with open(p, "wb") as f:
            f.write(blob)
        return p

    def test_rejects_unsupported_version(self):
        with tempfile.TemporaryDirectory() as d:
            p = self._write(d, _header(99, 1))
            with self.assertRaisesRegex(ValueError, "unsupported eNI model version"):
                read_eni_model(p)

    def test_rejects_zero_layer_count(self):
        with tempfile.TemporaryDirectory() as d:
            p = self._write(d, _header(VERSION_FLOAT, 0))
            with self.assertRaisesRegex(ValueError, "implausible layer count"):
                read_eni_model(p)

    def test_rejects_implausible_weight_count(self):
        with tempfile.TemporaryDirectory() as d:
            blob = _header(VERSION_FLOAT, 1) + _layer_header("l", 0, 5)
            p = self._write(d, blob)
            with self.assertRaisesRegex(ValueError, "implausible weight count"):
                read_eni_model(p)

    def test_reads_quantized_v2(self):
        # The v2 path the re-quantize refusal depends on.
        with tempfile.TemporaryDirectory() as d:
            inp = os.path.join(d, "q.eni_model")
            write_eni_model(
                inp,
                [
                    {
                        "name": "dense_0",
                        "input_size": 4,
                        "output_size": 2,
                        "activation": 1,
                        "weights": [1, -2, 3, -4, 5, -6, 7, -8],
                        "scale": 0.1,
                        "zero_point": 0,
                    }
                ],
                quantized=True,
            )
            model = read_eni_model(inp)
            self.assertEqual(model["version"], VERSION_QUANTIZED)
            self.assertEqual(len(model["layers"]), 1)


class TestMainValidation(unittest.TestCase):
    def _write_float_model(self, path, weights):
        write_eni_model(
            path,
            [
                {
                    "name": "dense_0",
                    "input_size": 4,
                    "output_size": 2,
                    "activation": 1,
                    "weights": weights,
                }
            ],
            quantized=False,
        )

    def test_refuses_already_quantized(self):
        with tempfile.TemporaryDirectory() as d:
            inp = os.path.join(d, "q.eni_model")
            out = os.path.join(d, "o.q")
            write_eni_model(
                inp,
                [
                    {
                        "name": "dense_0",
                        "input_size": 4,
                        "output_size": 2,
                        "activation": 1,
                        "weights": [1, -2, 3, -4, 5, -6, 7, -8],
                        "scale": 0.1,
                        "zero_point": 0,
                    }
                ],
                quantized=True,
            )
            with self.assertRaises(SystemExit) as cm:
                main(["-i", inp, "-o", out])
            self.assertNotEqual(cm.exception.code, 0)
            self.assertFalse(os.path.exists(out))

    @unittest.skipIf(HAS_ONNX, "needs the onnx package to be absent")
    def test_info_onnx_without_onnx_package(self):
        with tempfile.TemporaryDirectory() as d:
            inp = os.path.join(d, "m.onnx")
            out = os.path.join(d, "o.q")
            with open(inp, "wb") as f:
                f.write(b"not a real onnx file")
            with self.assertRaises(SystemExit) as cm:
                main(["-i", inp, "-o", out, "--info"])
            self.assertNotEqual(cm.exception.code, 0)
            self.assertFalse(os.path.exists(out))

    def test_output_validation_rejects_zero_weights(self):
        # A quantizer that emits all-zero weights must fail closed, and any
        # partial output file must be deleted.
        with tempfile.TemporaryDirectory() as d:
            inp = os.path.join(d, "m.eni_model")
            out = os.path.join(d, "o.q")
            self._write_float_model(inp, [0.1, -0.5, 0.9, -0.05, 0.33, 1.5, -1.2, 0.01])
            with open(out, "wb") as f:
                f.write(b"partial")
            with patch(
                "tools.quantize_model.quantize_weights",
                return_value=([0] * 8, 0.1, 0),
            ), self.assertRaises(SystemExit) as cm:
                main(["-i", inp, "-o", out])
            self.assertNotEqual(cm.exception.code, 0)
            self.assertFalse(os.path.exists(out))

    def test_output_validation_rejects_bad_dequant(self):
        # Quantized weights whose dequantization drifts beyond one scale
        # step are a corrupt artifact: refuse to write.
        with tempfile.TemporaryDirectory() as d:
            inp = os.path.join(d, "m.eni_model")
            out = os.path.join(d, "o.q")
            self._write_float_model(inp, [0.1, -0.5, 0.9, -0.05, 0.33, 1.5, -1.2, 0.01])
            with patch(
                "tools.quantize_model.quantize_weights",
                return_value=([10] * 8, 0.1, 0),
            ), patch(
                "tools.quantize_model.dequantize_weights",
                return_value=[999.0] * 8,
            ), self.assertRaises(SystemExit) as cm:
                main(["-i", inp, "-o", out])
            self.assertNotEqual(cm.exception.code, 0)
            self.assertFalse(os.path.exists(out))


if __name__ == "__main__":
    unittest.main()
