#!/usr/bin/env python3
"""eNI Model Quantization Tool — Convert float32 eNI models to INT8.

Fail-closed by design (eNI#40): the tool either quantizes the model it was
given or refuses with a non-zero exit. It never writes a placeholder model
while reporting success.
"""

import argparse
import os
import struct
import sys

MODEL_MAGIC = 0x454E4931
VERSION_FLOAT = 1
VERSION_QUANTIZED = 2


def quantize_weights(weights, bits=8):
    """Quantize float32 weights to INT8 symmetric."""
    if not weights:
        return [], 1.0, 0

    w_min = min(weights)
    w_max = max(weights)
    abs_max = max(abs(w_min), abs(w_max))

    if abs_max == 0:
        return [0] * len(weights), 1.0, 0

    qmax = (1 << (bits - 1)) - 1
    scale = abs_max / qmax

    quantized = []
    for w in weights:
        q = round(w / scale)
        q = max(-qmax, min(qmax, q))
        quantized.append(int(q))

    return quantized, scale, 0


def dequantize_weights(quantized, scale, zero_point=0):
    """Dequantize INT8 weights back to float32."""
    return [(q - zero_point) * scale for q in quantized]


def write_eni_model(path, layers, quantized=False):
    """Write a simple eNI model file."""
    magic = MODEL_MAGIC
    version = VERSION_QUANTIZED if quantized else VERSION_FLOAT

    with open(path, "wb") as f:
        f.write(struct.pack("<II", magic, version))
        f.write(struct.pack("<I", len(layers)))

        for layer in layers:
            name_bytes = layer["name"].encode("utf-8")[:31]
            f.write(struct.pack("<32s", name_bytes))
            f.write(struct.pack("<II", layer["input_size"], layer["output_size"]))
            f.write(struct.pack("<I", layer["activation"]))

            if quantized:
                f.write(struct.pack("<f", layer.get("scale", 1.0)))
                f.write(struct.pack("<i", layer.get("zero_point", 0)))
                for w in layer["weights"]:
                    f.write(struct.pack("<b", w))
            else:
                for w in layer["weights"]:
                    f.write(struct.pack("<f", w))


def read_eni_model(path):
    """Read an eNI model file (float v1 or quantized v2). Raises ValueError
    on bad magic, truncated data, or shape mismatches."""
    with open(path, "rb") as f:
        data = f.read()

    off = 0

    def take(n, what):
        nonlocal off
        chunk = data[off:off + n]
        if len(chunk) != n:
            raise ValueError(f"truncated file while reading {what}")
        off += n
        return chunk

    magic, version = struct.unpack("<II", take(8, "header"))
    if magic != MODEL_MAGIC:
        raise ValueError(f"bad magic 0x{magic:08x}, not an eNI model")
    if version not in (VERSION_FLOAT, VERSION_QUANTIZED):
        raise ValueError(f"unsupported eNI model version {version}")

    (n_layers,) = struct.unpack("<I", take(4, "layer count"))
    if n_layers == 0 or n_layers > 4096:
        raise ValueError(f"implausible layer count {n_layers}")

    layers = []
    for _ in range(n_layers):
        name = take(32, "layer name").split(b"\x00", 1)[0].decode("utf-8", "replace")
        input_size, output_size = struct.unpack("<II", take(8, "layer shape"))
        (activation,) = struct.unpack("<I", take(4, "activation"))
        n_weights = input_size * output_size
        if n_weights <= 0 or n_weights > 1 << 28:
            raise ValueError(f"implausible weight count {n_weights} in layer {name!r}")

        if version == VERSION_QUANTIZED:
            scale = struct.unpack("<f", take(4, "scale"))[0]
            zero_point = struct.unpack("<i", take(4, "zero_point"))[0]
            weights = list(struct.unpack(f"<{n_weights}b", take(n_weights, "weights")))
        else:
            scale, zero_point = 1.0, 0
            weights = list(struct.unpack(f"<{n_weights}f", take(4 * n_weights, "weights")))

        layers.append({
            "name": name,
            "input_size": input_size,
            "output_size": output_size,
            "activation": activation,
            "weights": weights,
            "scale": scale,
            "zero_point": zero_point,
        })

    return {"version": version, "layers": layers}


def _fail(msg):
    print(f"  ERROR: {msg}", file=sys.stderr)
    sys.exit(2)


def main(argv=None):
    parser = argparse.ArgumentParser(description="eNI Model Quantization Tool")
    parser.add_argument("--input", "-i", required=True, help="Input model path (.onnx or .eni_model)")
    parser.add_argument("--output", "-o", required=True, help="Output quantized model path")
    parser.add_argument("--bits", type=int, default=8, choices=[4, 8], help="Quantization bits (default: 8)")
    parser.add_argument("--calibration-data", help="Calibration data file (.npy)")
    parser.add_argument("--info", action="store_true", help="Show model info without quantizing")
    args = parser.parse_args(argv)

    print("eNI Quantization Tool v0.3.0")
    print(f"  Input:  {args.input}")
    print(f"  Output: {args.output}")
    print(f"  Bits:   {args.bits}")

    if not os.path.isfile(args.input):
        _fail(f"input not found: {args.input}")

    if args.input.endswith(".onnx"):
        # Real ONNX quantization needs onnxruntime's quantize_static, which is
        # out of scope for this tool. Refuse rather than emit a placeholder.
        if args.info:
            try:
                import onnx
            except ImportError:
                _fail("onnx package not installed; cannot show model info (pip install onnx)")
            model = onnx.load(args.input)
            print(f"  ONNX model: {len(model.graph.node)} nodes")
            for node in model.graph.node:
                print(f"    {node.op_type}: {node.name}")
            return
        _fail("ONNX quantization requires onnxruntime and is not supported yet; "
              "refusing to emit a placeholder model")

    # .eni_model path: read the actual float model and quantize it.
    try:
        model = read_eni_model(args.input)
    except (OSError, ValueError, struct.error) as e:
        _fail(f"cannot parse eNI model {args.input}: {e}")

    if model["version"] == VERSION_QUANTIZED:
        _fail(f"input {args.input} is already quantized (v2); refusing to re-quantize")

    layers = model["layers"]
    print(f"  Read {len(layers)} float layer(s) from eNI model")

    if args.info:
        for layer in layers:
            print(f"    {layer['name']}: {layer['input_size']}x{layer['output_size']} "
                  f"activation={layer['activation']}")
        return

    # Entry validation: at least one usable (non-zero) weight tensor, or the
    # output would be meaningless. Fail closed, emit nothing.
    if not any(any(w != 0 for w in layer["weights"]) for layer in layers):
        _fail("input has no usable weights (all zero); refusing to quantize")

    quantized_layers = []
    for layer in layers:
        q, scale, zp = quantize_weights(layer["weights"], bits=args.bits)
        quantized_layers.append({
            "name": layer["name"],
            "input_size": layer["input_size"],
            "output_size": layer["output_size"],
            "activation": layer["activation"],
            "weights": q,
            "scale": scale,
            "zero_point": zp,
        })

    # Output validation: the quantized model must actually encode the input.
    # A placeholder-shaped (all-zero) output is a defect — delete any partial
    # output and fail closed.
    def _output_valid():
        for src, dst in zip(layers, quantized_layers):
            if all(w == 0 for w in dst["weights"]) and not all(w == 0 for w in src["weights"]):
                return False
            back = dequantize_weights(dst["weights"], dst["scale"], dst["zero_point"])
            if any(abs(w - b) > dst["scale"] for w, b in zip(src["weights"], back)):
                return False
        return True

    if not _output_valid():
        if os.path.exists(args.output):
            os.remove(args.output)
        _fail("quantization produced invalid output; refusing to write it")

    write_eni_model(args.output, quantized_layers, quantized=True)
    print(f"  Quantized model written to: {args.output}")


if __name__ == "__main__":
    main()
