# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
"""EDF and XDF data format helpers using ctypes to call libeni."""

import ctypes
import ctypes.util
import os
import platform
from pathlib import Path
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# Library loading
# ---------------------------------------------------------------------------

def _find_libeni() -> Optional[str]:
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
    return found


_lib: Optional[ctypes.CDLL] = None


def _get_lib() -> ctypes.CDLL:
    """Lazily load and cache the native library."""
    global _lib
    if _lib is None:
        path = _find_libeni()
        if path is None:
            raise OSError(
                "Cannot find libeni_common. Build the project first or set "
                "LD_LIBRARY_PATH / DYLD_LIBRARY_PATH."
            )
        _lib = ctypes.CDLL(path)
    return _lib


# ---------------------------------------------------------------------------
# C structure definitions for EDF
# ---------------------------------------------------------------------------

ENI_DATA_MAX_CHANNELS = 256
ENI_DATA_MAX_LABEL = 80
ENI_EDF_MAX_ANNOTATIONS = 1024


class _ChannelInfo(ctypes.Structure):
    _fields_ = [
        ("label", ctypes.c_char * ENI_DATA_MAX_LABEL),
        ("transducer", ctypes.c_char * ENI_DATA_MAX_LABEL),
        ("physical_dim", ctypes.c_char * 8),
        ("physical_min", ctypes.c_double),
        ("physical_max", ctypes.c_double),
        ("digital_min", ctypes.c_int32),
        ("digital_max", ctypes.c_int32),
        ("sample_rate", ctypes.c_double),
        ("samples_per_record", ctypes.c_int),
    ]


class _DataHeader(ctypes.Structure):
    _fields_ = [
        ("format", ctypes.c_int),
        ("patient", ctypes.c_char * 80),
        ("recording", ctypes.c_char * 80),
        ("start_date", ctypes.c_char * 16),
        ("start_time", ctypes.c_char * 16),
        ("num_channels", ctypes.c_int),
        ("record_duration", ctypes.c_double),
        ("num_records", ctypes.c_int64),
        ("channels", _ChannelInfo * ENI_DATA_MAX_CHANNELS),
    ]


class _EdfAnnotation(ctypes.Structure):
    _fields_ = [
        ("onset", ctypes.c_double),
        ("duration", ctypes.c_double),
        ("text", ctypes.c_char * 256),
    ]


class _EdfFile(ctypes.Structure):
    _fields_ = [
        ("header", _DataHeader),
        ("annotations", _EdfAnnotation * ENI_EDF_MAX_ANNOTATIONS),
        ("annotation_count", ctypes.c_int),
        ("fp", ctypes.c_void_p),
        ("writing", ctypes.c_bool),
        ("data_offset", ctypes.c_int64),
        ("record_size", ctypes.c_int),
    ]


# ---------------------------------------------------------------------------
# C structure definitions for XDF
# ---------------------------------------------------------------------------

ENI_XDF_MAX_STREAMS = 32


class _XdfStream(ctypes.Structure):
    _fields_ = [
        ("stream_id", ctypes.c_int),
        ("name", ctypes.c_char * 80),
        ("type", ctypes.c_char * 32),
        ("channel_count", ctypes.c_int),
        ("nominal_srate", ctypes.c_double),
        ("channel_format", ctypes.c_char * 16),
        ("clock_offsets", ctypes.c_double * 64),
        ("clock_offset_count", ctypes.c_int),
        ("sample_count", ctypes.c_int64),
    ]


class _XdfFile(ctypes.Structure):
    _fields_ = [
        ("streams", _XdfStream * ENI_XDF_MAX_STREAMS),
        ("stream_count", ctypes.c_int),
        ("fp", ctypes.c_void_p),
        ("writing", ctypes.c_bool),
        ("file_header", ctypes.c_char * 1024),
    ]


# ---------------------------------------------------------------------------
# EDF read / write
# ---------------------------------------------------------------------------

def read_edf(path: str) -> Dict[str, Any]:
    """Read an EDF+ file via libeni.

    Args:
        path: Path to the .edf file.

    Returns:
        Dict with keys:
            header: dict with patient, recording, start_date, start_time,
                    num_channels, record_duration, num_records, channels.
            records: list of lists (one per record) of float sample values.
            annotations: list of dicts with onset, duration, text.
    """
    lib = _get_lib()

    edf = _EdfFile()
    c_path = path.encode("utf-8")
    rc = lib.eni_edf_open(ctypes.byref(edf), c_path)
    if rc != 0:
        raise OSError(f"eni_edf_open failed for '{path}' (status {rc})")

    try:
        hdr = edf.header
        num_ch = hdr.num_channels
        num_records = hdr.num_records

        # Parse channel info
        channels = []
        for i in range(num_ch):
            ch = hdr.channels[i]
            channels.append({
                "label": ch.label.decode("utf-8", errors="replace").strip(),
                "physical_dim": ch.physical_dim.decode("utf-8", errors="replace").strip(),
                "physical_min": ch.physical_min,
                "physical_max": ch.physical_max,
                "digital_min": ch.digital_min,
                "digital_max": ch.digital_max,
                "sample_rate": ch.sample_rate,
                "samples_per_record": ch.samples_per_record,
            })

        header_dict = {
            "patient": hdr.patient.decode("utf-8", errors="replace").strip(),
            "recording": hdr.recording.decode("utf-8", errors="replace").strip(),
            "start_date": hdr.start_date.decode("utf-8", errors="replace").strip(),
            "start_time": hdr.start_time.decode("utf-8", errors="replace").strip(),
            "num_channels": num_ch,
            "record_duration": hdr.record_duration,
            "num_records": num_records,
            "channels": channels,
        }

        # Read data records
        total_samples_per_record = sum(
            ch["samples_per_record"] for ch in channels
        ) or num_ch
        buf_type = ctypes.c_double * total_samples_per_record

        records: List[List[float]] = []
        for rec_idx in range(num_records):
            buf = buf_type()
            rc = lib.eni_edf_read_record(
                ctypes.byref(edf),
                ctypes.c_int64(rec_idx),
                buf,
                ctypes.c_int(total_samples_per_record),
            )
            if rc != 0:
                break
            records.append(list(buf))

        # Read annotations
        lib.eni_edf_read_annotations(ctypes.byref(edf))
        annotations = []
        for i in range(edf.annotation_count):
            ann = edf.annotations[i]
            annotations.append({
                "onset": ann.onset,
                "duration": ann.duration,
                "text": ann.text.decode("utf-8", errors="replace").strip(),
            })

        return {
            "header": header_dict,
            "records": records,
            "annotations": annotations,
        }
    finally:
        lib.eni_edf_close(ctypes.byref(edf))


def write_edf(
    path: str,
    header: Dict[str, Any],
    records: List[List[float]],
    annotations: Optional[List[Dict[str, Any]]] = None,
) -> None:
    """Write an EDF+ file via libeni.

    Args:
        path: Output file path.
        header: Dict with patient, recording, start_date, start_time,
                num_channels, record_duration, channels list.
        records: List of sample lists (one per data record).
        annotations: Optional list of annotation dicts (onset, duration, text).
    """
    lib = _get_lib()

    hdr = _DataHeader()
    hdr.format = 1  # ENI_FORMAT_EDF
    hdr.patient = header.get("patient", "").encode("utf-8")[:79]
    hdr.recording = header.get("recording", "").encode("utf-8")[:79]
    hdr.start_date = header.get("start_date", "").encode("utf-8")[:15]
    hdr.start_time = header.get("start_time", "").encode("utf-8")[:15]
    hdr.num_channels = header.get("num_channels", 0)
    hdr.record_duration = header.get("record_duration", 1.0)
    hdr.num_records = len(records)

    channels = header.get("channels", [])
    for i, ch in enumerate(channels):
        if i >= ENI_DATA_MAX_CHANNELS:
            break
        hdr.channels[i].label = ch.get("label", "").encode("utf-8")[:ENI_DATA_MAX_LABEL - 1]
        hdr.channels[i].physical_dim = ch.get("physical_dim", "uV").encode("utf-8")[:7]
        hdr.channels[i].physical_min = ch.get("physical_min", -3200.0)
        hdr.channels[i].physical_max = ch.get("physical_max", 3200.0)
        hdr.channels[i].digital_min = ch.get("digital_min", -32768)
        hdr.channels[i].digital_max = ch.get("digital_max", 32767)
        hdr.channels[i].sample_rate = ch.get("sample_rate", 256.0)
        hdr.channels[i].samples_per_record = ch.get("samples_per_record", 256)

    edf = _EdfFile()
    c_path = path.encode("utf-8")
    rc = lib.eni_edf_create(ctypes.byref(edf), c_path, ctypes.byref(hdr))
    if rc != 0:
        raise OSError(f"eni_edf_create failed for '{path}' (status {rc})")

    try:
        for record in records:
            n = len(record)
            buf = (ctypes.c_double * n)(*record)
            rc = lib.eni_edf_write_record(ctypes.byref(edf), buf, ctypes.c_int(n))
            if rc != 0:
                raise OSError(f"eni_edf_write_record failed (status {rc})")

        if annotations:
            for ann in annotations:
                text = ann.get("text", "").encode("utf-8")
                rc = lib.eni_edf_write_annotation(
                    ctypes.byref(edf),
                    ctypes.c_double(ann.get("onset", 0.0)),
                    ctypes.c_double(ann.get("duration", 0.0)),
                    text,
                )
                if rc != 0:
                    raise OSError(f"eni_edf_write_annotation failed (status {rc})")

        rc = lib.eni_edf_finalize(ctypes.byref(edf))
        if rc != 0:
            raise OSError(f"eni_edf_finalize failed (status {rc})")
    finally:
        lib.eni_edf_close(ctypes.byref(edf))


# ---------------------------------------------------------------------------
# XDF read
# ---------------------------------------------------------------------------

def read_xdf(path: str) -> Dict[str, Any]:
    """Read an XDF file via libeni.

    Args:
        path: Path to the .xdf file.

    Returns:
        Dict with keys:
            streams: list of dicts per stream (stream_id, name, type,
                     channel_count, nominal_srate, channel_format, samples).
    """
    lib = _get_lib()

    xdf = _XdfFile()
    c_path = path.encode("utf-8")
    rc = lib.eni_xdf_open(ctypes.byref(xdf), c_path)
    if rc != 0:
        raise OSError(f"eni_xdf_open failed for '{path}' (status {rc})")

    try:
        streams = []
        for i in range(xdf.stream_count):
            s = xdf.streams[i]
            max_samples = max(int(s.sample_count * s.channel_count), 1)
            buf = (ctypes.c_double * max_samples)()
            samples_read = ctypes.c_int(0)

            rc = lib.eni_xdf_read_samples(
                ctypes.byref(xdf),
                ctypes.c_int(s.stream_id),
                buf,
                ctypes.c_int(max_samples),
                ctypes.byref(samples_read),
            )

            samples = list(buf[: samples_read.value]) if rc == 0 else []

            streams.append({
                "stream_id": s.stream_id,
                "name": s.name.decode("utf-8", errors="replace").strip(),
                "type": s.type.decode("utf-8", errors="replace").strip(),
                "channel_count": s.channel_count,
                "nominal_srate": s.nominal_srate,
                "channel_format": s.channel_format.decode("utf-8", errors="replace").strip(),
                "sample_count": s.sample_count,
                "samples": samples,
            })

        return {"streams": streams}
    finally:
        lib.eni_xdf_close(ctypes.byref(xdf))
