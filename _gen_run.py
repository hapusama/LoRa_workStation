import os

target = r"D:\Desktop\proj\gr-lora_sdr\weakPacket_decoding copy\scripts\run_phase_guided_demod.py"

parts = []

parts.append('''#!/usr/bin/env python3
"""Phase-Guided FFT Demod: 与 argmax baseline 对比的实验入口。

从 sync_chain CSV 读取 framesync-valid 的 packet,
用 phase-guided demod 重新解调 payload FFT bin,
与 header-first 产出的 GT bin 对比, 评估 accuracy 提升。

用法:
  python scripts/run_phase_guided_demod.py
    -i data/USRP_IQ/0_0_0_10_14_16.bin
    -s weakPacket_decoding/data/weak_sync_chain/sync_chain/0_0_0_10_14_16_sync_chain.csv
    -g weakPacket_decoding/data/weak_sync_chain/header_first/0_0_0_10_14_16_header_first_symbols.csv
    -o weakPacket_decoding/data/phase_guided/
    --sf 10 --bw 125000 --samp-rate 500000 --preamble-len 16
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
import sys
from typing import Any, Optional

import numpy as np


WEAK_ROOT = Path(__file__).resolve().parent.parent
if str(WEAK_ROOT) not in sys.path:
    sys.path.insert(0, str(WEAK_ROOT))

from weak_decoder.chirp import build_downchirp, positive_mod
from weak_decoder.header_first_demod import (
    decode_explicit_header,
    demod_one_symbol,
    demod_symbol_sequence,
    resolve_ldro,
    payload_symbol_count,
)
from weak_decoder.phase_guided_demod import (
    PhaseGuidedPayloadConfig,
    PhaseLine,
    phase_guided_demod_packet,
    PayloadSymbolDecision,
    fit_phase_line,
    robust_fit_phase_line,
)
''')

parts.append('''

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Phase-guided FFT bin selection LoRa demod.")
    parser.add_argument("-i", "--input", type=Path, required=True, help="complex64 IQ .bin")
    parser.add_argument("-s", "--sync-chain-csv", type=Path, required=True, help="sync_chain CSV")
    parser.add_argument("-g", "--gt-symbol-csv", type=Path, default=None, help="header-first symbol CSV (GT bins)")
    parser.add_argument("-o", "--output-dir", type=Path, required=True, help="output directory")
    parser.add_argument("--sf", type=int, default=10, help="spreading factor")
    parser.add_argument("--bw", type=float, default=125000.0, help="bandwidth Hz")
    parser.add_argument("--samp-rate", type=float, default=500000.0, help="IQ sample rate")
    parser.add_argument("--preamble-len", type=float, default=8.0, help="preamble length in symbols")
    parser.add_argument("--os-factor", type=int, default=4, help="oversampling factor")
    parser.add_argument("--packet", type=int, nargs="*", default=None, help="only specific packet indices")
    parser.add_argument("--max-packets", type=int, default=None, help="max packets to process")
    parser.add_argument("--top-l", type=int, default=32, help="Top-L candidates per symbol")
    parser.add_argument("--argmax-window", type=int, default=2, help="argmax neighborhood radius")
    parser.add_argument("--refinement-rounds", type=int, default=3, help="max refinement rounds")
    parser.add_argument("--trim-frac", type=float, default=0.25, help="trim fraction for robust fit")
    parser.add_argument("--confidence-threshold", type=float, default=0.3, help="pseudo-anchor threshold")
    parser.add_argument("--cfo-correction-mode", choices=("symbol", "continuous"), default="continuous")
    parser.add_argument("--skip-header-anchor", action="store_true", default=False,
                        help="do not use header symbols as anchors")
    parser.add_argument("--seed", type=int, default=None, help="random seed")
    return parser.parse_args()
''')

parts.append('''
def _int(row: dict[str, str], key: str, default: int = 0) -> int:
    v = str(row.get(key, "")).strip()
    return int(float(v)) if v else int(default)

def _float(row: dict[str, str], key: str, default: float = float("nan")) -> float:
    v = str(row.get(key, "")).strip()
    return float(v) if v else float(default)

def _flag(row: dict[str, str], key: str, default: bool = False) -> bool:
    v = str(row.get(key, "")).strip()
    return bool(int(float(v))) if v else bool(default)

def _group_key(row: dict[str, str]) -> tuple[int, int]:
    return (_int(row, "packet_index", -1), _int(row, "event_index", -1))

def read_sync_candidates(path: Path, packet_filter: Optional[set[int]], max_packets: Optional[int]) -> list[dict]:
    candidates = []
    with path.open("r", encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if not _flag(row, "grlora_framesync_valid", False):
                continue
            pk = _int(row, "packet_index", -1)
            if packet_filter is not None and pk not in packet_filter:
                continue
            candidates.append(dict(row))
    candidates.sort(key=lambda r: (_int(r, "packet_index"), _int(r, "event_index")))
    if max_packets is not None:
        candidates = candidates[: int(max_packets)]
    return candidates

def read_gt_symbols(path: Path, packet_filter: Optional[set[int]]) -> dict[tuple[int, int], dict[int, int]]:
    """返回 {(packet_idx, event_idx): {payload_symbol_index: gt_raw_fft_bin}}"""
    result: dict[tuple[int, int], dict[int, int]] = {}
    with path.open("r", encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if _int(row, "stage") != "payload" or _int(row, "header_valid", 0) != 1:
                continue
            key = _group_key(row)
            pk = _int(row, "packet_index", -1)
            if packet_filter is not None and pk not in packet_filter:
                continue
            if key not in result:
                result[key] = {}
            sym_idx = _int(row, "stage_symbol_index", -1)
            gt_bin = _int(row, "raw_fft_bin", -1)
            if gt_bin >= 0:
                result[key][sym_idx] = gt_bin
    return result

def write_csv(path: Path, rows: list[dict[str, Any]], fieldnames: Optional[list[str]] = None) -> None:
    if not rows:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fn = fieldnames or list(rows[0].keys())
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fn)
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in fn})
''')

parts.append('''
def main() -> int:
    args = parse_args()
    if args.seed is not None:
        np.random.seed(int(args.seed))

    input_path = args.input.resolve()
    sync_csv = args.sync_chain_csv.resolve()
    gt_csv = args.gt_symbol_csv.resolve() if args.gt_symbol_csv else None
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    samples = np.fromfile(input_path, dtype=np.complex64)
    if samples.size == 0:
        raise ValueError(f"Empty IQ file: {input_path}")

    sf = int(args.sf)
    os_factor = int(args.os_factor)
    n_bins = 1 << sf
    chirp_samples = n_bins * os_factor
    preamble_len = float(args.preamble_len)

    packet_filter = set(args.packet) if args.packet else None
    candidates = read_sync_candidates(sync_csv, packet_filter=packet_filter, max_packets=args.max_packets)
    gt_map = read_gt_symbols(gt_csv, packet_filter=packet_filter) if gt_csv else {}

    config = PhaseGuidedPayloadConfig(
        top_l=int(args.top_l),
        neighborhood_radius=0,
        max_refinement_rounds=int(args.refinement_rounds),
        trim_frac=float(args.trim_frac),
        confidence_threshold=float(args.confidence_threshold),
        argmax_window_radius=int(args.argmax_window),
        use_header_anchor=not bool(args.skip_header_anchor),
    )

    summary_rows: list[dict[str, Any]] = []
    symbol_rows: list[dict[str, Any]] = []
    print(f"Processing {len(candidates)} framesync-valid packets...")

    for idx, cand in enumerate(candidates):
        packet_idx = _int(cand, "packet_index")
        event_idx = _int(cand, "event_index")
        fine_payload_start = _int(cand, "grlora_fine_payload_start_sample")
        cfo_int = _int(cand, "grlora_cfo_int_est", 0)
        cfo_frac = _float(cand, "grlora_cfo_frac_est", 0.0)
        sfo_hat = _float(cand, "grlora_sfo_hat", 0.0)
        cfo_correction_mode = str(args.cfo_correction_mode)

        # Demod header to get header symbols and decode
        downchirp = build_downchirp(sf, cfo_int=cfo_int, cfo_frac=cfo_frac)
        cfo_total = float(cfo_int) + float(cfo_frac)

        # Demod 8 header symbols using argmax + decode
        header_results = demod_symbol_sequence(
            samples=samples,
            header_start_sample=int(fine_payload_start),
            sf=sf,
            os_factor=os_factor,
            cfo_int=cfo_int,
            cfo_frac=cfo_frac,
            sfo_hat=sfo_hat,
            sfo_cum_initial=0.0,
            header_count=8,
            payload_count=0,
            payload_ldro=False,
            cfo_correction_mode=cfo_correction_mode,
        )

        # Decode header
        if len(header_results) < 8:
            print(f"  Packet {packet_idx}, event {event_idx}: header demod incomplete, skipping")
            continue

        header_symbol_values = [r.symbol_value for r in header_results]
        header_decode = decode_explicit_header(
            header_symbol_values,
            sf=sf,
            bw=float(args.bw),
            ldro_mode=2,
        )

        if not header_decode.header_valid:
            print(f"  Packet {packet_idx}, event {event_idx}: header invalid, skipping")
            continue

        payload_ldro = header_decode.ldro
        payload_count = header_decode.payload_symbol_count
        header_payload_len = header_decode.payload_len
        header_cr = header_decode.cr
        header_has_crc = header_decode.has_crc

        # Get GT bins for this packet
        gt_bins = gt_map.get((packet_idx, event_idx), {}) if gt_map else {}

        # Run phase-guided demod
        result = phase_guided_demod_packet(
            samples=samples,
            header_start_sample=int(fine_payload_start),
            sf=sf,
            os_factor=os_factor,
            cfo_int=cfo_int,
            cfo_frac=cfo_frac,
            sfo_hat=sfo_hat,
            preamble_len=preamble_len,
            header_symbol_values=header_symbol_values,
            header_payload_len=header_payload_len,
            header_cr=header_cr,
            header_has_crc=header_has_crc,
            header_ldro=payload_ldro,
            payload_symbol_count=payload_count,
            config=config,
            cfo_correction_mode=cfo_correction_mode,
            gt_bins=gt_bins,
        )

        # Build summary row
        il = result.get("initial_phase_line", PhaseLine())
        fl = result.get("final_phase_line", PhaseLine())
        round_acc = result.get("round_accuracy", [])
        ser_final = result.get("final_error_rate", 1.0)
        ser_initial = round_acc[0] if round_acc else 0.0
        ser_last = round_acc[-1] if round_acc else 0.0
        argmax_ser = 1.0 - ser_last if ser_last > 0 else 1.0

        summary_rows.append({
            "packet_index": packet_idx,
            "event_index": event_idx,
            "success": int(result.get("success", False)),
            "total_payload": result.get("total_payload", 0),
            "correct_count": result.get("correct_count", 0),
            "final_error_rate": f"{ser_final:.4f}",
            "initial_slope_pi": f"{il.slope_pi:.4f}",
            "final_slope_pi": f"{fl.slope_pi:.4f}",
            "initial_line_r2": f"{il.fit_r2:.4f}",
            "final_line_r2": f"{fl.fit_r2:.4f}",
            "preamble_anchors": result.get("preamble_anchor_count", 0),
            "header_anchors": result.get("header_anchor_count", 0),
            "refinement_rounds": result.get("refinement_rounds", 0),
            "round0_accuracy": f"{round_acc[0]:.4f}" if len(round_acc) > 0 else "",
            "round1_accuracy": f"{round_acc[1]:.4f}" if len(round_acc) > 1 else "",
            "round2_accuracy": f"{round_acc[2]:.4f}" if len(round_acc) > 2 else "",
            "payload_len": header_payload_len,
            "gt_bins_available": int(len(gt_bins)),
        })

        # Build per-symbol rows
        for dec in result.get("payload_decisions", []):
            symbol_rows.append({
                "packet_index": packet_idx,
                "event_index": event_idx,
                "symbol_index": dec.frame_symbol_index,
                "selected_bin": dec.raw_fft_bin,
                "signed_bin": dec.signed_fft_bin,
                "symbol_value": dec.symbol_value,
                "phase_at_bin": f"{dec.phase_at_bin:.6f}",
                "phase_predicted": f"{dec.phase_predicted:.6f}",
                "phase_residual_rad": f"{dec.phase_residual_rad:.6f}",
                "phase_score": f"{dec.phase_score:.6f}",
                "margin": f"{dec.margin:.6f}",
                "confidence": f"{dec.confidence:.4f}",
                "amplitude": f"{dec.amplitude:.6f}",
                "energy_ratio": f"{dec.energy_ratio:.6f}",
                "argmax_bin": dec.argmax_bin,
                "argmax_is_selected": int(dec.argmax_is_selected),
                "rank_by_amp": dec.rank_by_amp,
                "gt_bin": dec.gt_bin,
                "is_correct": int(dec.is_correct),
            })

        print(f"  [{idx+1}/{len(candidates)}] Pkt {packet_idx} E{event_idx}: "
              f"payload={result.get('total_payload', 0):3d} "
              f"correct={result.get('correct_count', 0):3d} "
              f"SER={ser_final:.4f} "
              f"rounds={result.get('refinement_rounds', 0):1d} "
              f"slope={fl.slope_pi:.4f}pi/sym")

    # Write output
    summary_path = output_dir / f"{input_path.stem}_phase_guided_summary.csv"
    symbol_path = output_dir / f"{input_path.stem}_phase_guided_symbols.csv"
    write_csv(summary_path, summary_rows)
    write_csv(symbol_path, symbol_rows)

    # Print aggregate
    if gt_map and summary_rows:
        rates = [_float(r, "final_error_rate", 1.0) for r in summary_rows]
        mean_ser = sum(rates) / len(rates) if rates else 0.0
        print(f"\\nAggregate: {len(summary_rows)} packets, mean SER = {mean_ser:.4f}")
    print(f"Summary: {summary_path}")
    print(f"Symbols: {symbol_path}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
''')

code = "\n".join(parts)
with open(target, "w", encoding="utf-8") as f:
    f.write(code)
print(f"Written {len(code)} chars to {target}")
