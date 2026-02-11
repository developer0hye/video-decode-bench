#!/usr/bin/env python3
"""
Run video decoding benchmarks on all test videos and produce a summary report.

Runs inside the Docker container — no Docker-in-Docker needed.

Usage (from host):
    # Build image first (if needed)
    docker build -t video-bench-dev -f docker/Dockerfile .

    # One-shot: build + benchmark
    docker run --rm -v "$(pwd)":/app -w /app video-bench-dev \
        python3 scripts/run_full_benchmark.py --max-streams 32

    # Skip build if already compiled
    docker run --rm -v "$(pwd)":/app -w /app video-bench-dev \
        python3 scripts/run_full_benchmark.py --skip-build --max-streams 32
"""

import argparse
import csv
import subprocess
import sys
from datetime import datetime
from pathlib import Path

try:
    from tabulate import tabulate
except ImportError:
    print("Missing dependency: tabulate")
    print("Install with: apt-get install -y python3-tabulate")
    sys.exit(1)

PROJECT_DIR = Path(__file__).resolve().parent.parent
VIDEO_DIR = PROJECT_DIR / "test_videos"
BENCHMARK_BIN = PROJECT_DIR / "build" / "video-benchmark"

RESOLUTION_ORDER = ["hd", "fhd", "4k"]
RESOLUTION_LABELS = {"hd": "HD (720p)", "fhd": "FHD (1080p)", "4k": "4K (2160p)"}

CODEC_ORDER = ["h264", "h265", "vp9", "av1"]
CODEC_LABELS = {"h264": "H.264", "h265": "H.265", "vp9": "VP9", "av1": "AV1"}


def build(skip: bool) -> None:
    if skip:
        print("=== Skipping build (--skip-build) ===\n")
        return

    print("=== Building project ===")
    build_dir = PROJECT_DIR / "build"
    build_dir.mkdir(exist_ok=True)
    subprocess.run(
        ["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"],
        cwd=build_dir, check=True,
    )
    subprocess.run(
        ["make", "-j"],
        cwd=build_dir, check=True,
    )
    print()


def discover_videos() -> list[Path]:
    videos = sorted(
        list(VIDEO_DIR.glob("test_video_*.mp4"))
        + list(VIDEO_DIR.glob("test_video_*.webm"))
    )
    if not videos:
        print(f"Error: No test videos found in {VIDEO_DIR}")
        sys.exit(1)
    return videos


def parse_video_name(path: Path) -> tuple[str, str]:
    """Extract (resolution, codec) from filename like test_video_hd_h264.mp4."""
    parts = path.stem.split("_")  # ['test', 'video', 'hd', 'h264']
    return parts[2], parts[3]


def run_benchmark(
    video: Path, max_streams: int, result_dir: Path,
) -> dict | None:
    """Run benchmark for a single video and return the best passing result."""
    resolution, codec = parse_video_name(video)
    key = f"{resolution}_{codec}"
    csv_path = result_dir / f"{key}.csv"
    log_path = result_dir / f"{key}.log"

    subprocess.run(
        [
            str(BENCHMARK_BIN),
            "--max-streams", str(max_streams),
            "--csv-file", str(csv_path),
            "--log-file", str(log_path),
            str(video),
        ],
        check=False,
    )

    if not csv_path.exists():
        return None

    best = None
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            if row["passed"] == "true":
                best = row
    return best


def read_system_info(result_dir: Path) -> tuple[str, str]:
    """Read CPU and RAM info from the first available log file."""
    cpu_info, ram_info = "", ""
    for log in sorted(result_dir.glob("*.log")):
        for line in log.read_text().splitlines():
            if "CPU:" in line and not cpu_info:
                cpu_info = line[line.index("CPU:"):]
            if "RAM:" in line and not ram_info:
                ram_info = line[line.index("RAM:"):]
            if cpu_info and ram_info:
                return cpu_info, ram_info
    return cpu_info, ram_info


def fmt(val: str | None, as_int: bool = False) -> str:
    if val is None or val == "-":
        return "-"
    if as_int:
        try:
            return str(int(float(val)))
        except ValueError:
            return val
    return val


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run full video decoding benchmark suite",
    )
    parser.add_argument(
        "--max-streams", type=int, default=64,
        help="Maximum streams to test per video (default: 64)",
    )
    parser.add_argument(
        "--skip-build", action="store_true",
        help="Skip cmake/make build step",
    )
    args = parser.parse_args()

    # Verify binary exists when skipping build
    if args.skip_build and not BENCHMARK_BIN.exists():
        print(f"Error: {BENCHMARK_BIN} not found. Run without --skip-build first.")
        sys.exit(1)

    timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
    result_dir = PROJECT_DIR / "benchmark_results" / timestamp
    result_dir.mkdir(parents=True, exist_ok=True)

    build(args.skip_build)

    videos = discover_videos()
    total = len(videos)
    print(f"=== Found {total} test videos | Max streams: {args.max_streams} ===\n")

    results: dict[str, dict | None] = {}

    for i, video in enumerate(videos, 1):
        resolution, codec = parse_video_name(video)
        res_label = RESOLUTION_LABELS.get(resolution, resolution)
        codec_label = CODEC_LABELS.get(codec, codec)

        print(f"[{i}/{total}] {res_label} {codec_label} ({video.name})")
        print("-" * 60)
        results[f"{resolution}_{codec}"] = run_benchmark(
            video, args.max_streams, result_dir,
        )
        print()

    # ─── Summary ─────────────────────────────────────────────────────────

    cpu_info, ram_info = read_system_info(result_dir)

    header_lines = [
        "",
        "=" * 78,
        "VIDEO DECODING BENCHMARK SUMMARY".center(78),
        "=" * 78,
    ]
    if cpu_info:
        header_lines.append(f"  {cpu_info}")
    if ram_info:
        header_lines.append(f"  {ram_info}")
    header_lines.append(f"  Max streams tested: {args.max_streams}")
    header_lines.append(f"  Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    header_lines.append("-" * 78)

    table_rows = []
    for res in RESOLUTION_ORDER:
        for codec in CODEC_ORDER:
            key = f"{res}_{codec}"
            row = results.get(key)
            if row:
                table_rows.append([
                    RESOLUTION_LABELS[res],
                    CODEC_LABELS[codec],
                    fmt(row["stream_count"]),
                    fmt(row["avg_fps"], as_int=True),
                    fmt(row["cpu_usage"], as_int=True),
                    fmt(row["memory_mb"]),
                ])
            else:
                table_rows.append([
                    RESOLUTION_LABELS[res], CODEC_LABELS[codec],
                    "-", "-", "-", "-",
                ])

    table_headers = ["Resolution", "Codec", "Max Streams", "Avg FPS", "CPU %", "RAM (MB)"]
    table_str = tabulate(
        table_rows, headers=table_headers, tablefmt="simple_outline",
        colalign=("left", "left", "right", "right", "right", "right"),
    )

    output_lines = header_lines + [table_str, "=" * 78]
    summary_text = "\n".join(output_lines)
    print(summary_text)

    # Save files
    (result_dir / "summary.txt").write_text(summary_text + "\n")

    with open(result_dir / "summary.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["resolution", "codec", "max_streams", "avg_fps", "cpu_percent", "memory_mb"])
        for row_data in table_rows:
            writer.writerow(row_data)

    print(f"\nResults saved to: benchmark_results/{timestamp}/")
    print("  summary.txt  - Summary table")
    print("  summary.csv  - Machine-readable summary")
    print("  *.csv        - Per-video detailed results")
    print("  *.log        - Per-video benchmark logs")


if __name__ == "__main__":
    main()
