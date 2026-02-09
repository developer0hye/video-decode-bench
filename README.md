# Video Decode Benchmark

`video-benchmark` measures how many concurrent video streams your machine can decode in real time using CPU software decoding (no GPU offload).

## Key Features

- Real-time paced decoding to simulate actual playback timing
- Concurrent stream scaling test to find maximum sustainable stream count
- FPS and CPU threshold based pass/fail criteria
- Codec support: H.264, H.265/HEVC, VP9, AV1
- **RTSP stream support** for IP camera / network stream testing
- Result logging to `video-benchmark.log` (thread-safe logging via `spdlog`)

## Repository Structure

- `src/`: core benchmark source code
- `docker/Dockerfile`: development/build container image
- `test_videos/`: sample benchmark videos
- `build/`: CMake build output (generated)

## Requirements

- Docker 24+ recommended
- Git

## Run With Docker (Detailed)

### 1. Clone and enter the project

```bash
git clone <your-repo-url> video-decode-bench
cd video-decode-bench
```

### 2. Build the Docker development image

Run this from the repository root (where `CMakeLists.txt` is located):

```bash
docker build -t video-bench-dev -f docker/Dockerfile .
```

### 3. Start an interactive container

```bash
docker run --rm -it -v "$(pwd)":/app -w /app video-bench-dev bash
```

What this does:

- `--rm`: remove container when it exits
- `-it`: interactive shell
- `-v "$(pwd)":/app`: mounts your repo into the container
- `-w /app`: sets working directory inside container

### 4. Configure and build inside the container

```bash
cmake -S . -B build
cmake --build build -j
```

### 5. Run a benchmark

Example using a bundled sample video:

```bash
./build/video-benchmark test_videos/test_video_hd_h264.mp4
```

Example with options:

```bash
./build/video-benchmark --max-streams 16 --target-fps 30 test_videos/test_video_fhd_h265.mp4
```

### 6. Check the log file

Each run writes execution output to:

```bash
video-benchmark.log
```

Inside the container:

```bash
tail -n 50 video-benchmark.log
```

Because `/app` is a bind mount, this log file is also visible on your host machine at the project root.

## One-Shot Docker Command (No Interactive Shell)

Build and run in a single command:

```bash
docker run --rm -v "$(pwd)":/app -w /app video-bench-dev bash -lc \
  "cmake -S . -B build && cmake --build build -j && ./build/video-benchmark test_videos/test_video_hd_h264.mp4"
```

## CLI Usage

```bash
./build/video-benchmark [OPTIONS] <video_source>
```

The `<video_source>` can be a local file path or an RTSP URL.

Options:

- `-m, --max-streams N`: maximum number of streams to test
- `-f, --target-fps FPS`: target FPS threshold (default: source video FPS)
- `-h, --help`: show help
- `-v, --version`: show version

Examples:

```bash
# Local file
./build/video-benchmark test_videos/test_video_fhd_h264.mp4

# RTSP stream
./build/video-benchmark rtsp://192.168.1.100:554/stream

# With options
./build/video-benchmark --max-streams 8 rtsp://camera.local/live
```

## Running Your Own Video File

If your video is already in this repository, pass its path directly:

```bash
./build/video-benchmark path/to/your_video.mp4
```

If your video is outside the repository, mount that directory as another volume:

```bash
docker run --rm -it \
  -v "$(pwd)":/app \
  -v "/absolute/path/to/videos":/videos \
  -w /app \
  video-bench-dev bash
```

Then run:

```bash
./build/video-benchmark /videos/your_video.mp4
```

## RTSP Stream Testing

You can test with RTSP streams from IP cameras or set up a local RTSP server for testing.

**Supported codecs over RTSP: H.264 and H.265 only.** VP9 and AV1 are not supported over RTSP due to FFmpeg 6.1.1 RTP packetizer limitations (VP9 RTP is experimental/broken, AV1 RTP packetizer is not included). Use local file mode for VP9/AV1 benchmarks.

### Testing with Local RTSP Server

1. Start the RTSP server (mediamtx):

```bash
docker run --rm -d --name rtsp-server --network host bluenviron/mediamtx:latest
```

2. Stream a local video file to the RTSP server:

**H.264** (re-encoding required for stable timestamps):
```bash
docker run --rm -d --name rtsp-streamer --network host \
  -v "$(pwd)/test_videos":/videos video-bench-dev bash -c \
  "ffmpeg -re -f concat -safe 0 \
   -i <(for i in \$(seq 1 100); do echo \"file '/videos/test_video_fhd_h264.mp4'\"; done) \
   -c:v libx264 -preset ultrafast -tune zerolatency -g 30 -an \
   -f rtsp rtsp://127.0.0.1:8554/test"
```

**H.265** (copy mode works well):
```bash
docker run --rm -d --name rtsp-streamer --network host \
  -v "$(pwd)/test_videos":/videos video-bench-dev bash -c \
  "ffmpeg -re -f concat -safe 0 \
   -i <(for i in \$(seq 1 100); do echo \"file '/videos/test_video_fhd_h265.mp4'\"; done) \
   -c:v copy -an \
   -f rtsp rtsp://127.0.0.1:8554/test"
```

3. Run the benchmark:

```bash
docker run --rm --network host -v "$(pwd)":/app video-bench-dev \
  ./build/video-benchmark rtsp://127.0.0.1:8554/test --max-streams 8
```

4. Clean up:

```bash
docker stop rtsp-streamer rtsp-server
```

### Testing with IP Camera

Simply pass the camera's RTSP URL:

```bash
./build/video-benchmark rtsp://admin:password@192.168.1.100:554/stream1
```

Note: Each decoder thread opens its own RTSP connection, so the camera must support multiple concurrent connections.

## Native Build (Optional)

If you want to build outside Docker, install a C++20 toolchain, FFmpeg development libraries, and CMake, then:

```bash
cmake -S . -B build
cmake --build build -j
./build/video-benchmark test_videos/test_video_hd_h264.mp4
```

Note: if `spdlog` is not installed system-wide, CMake fetches it automatically during configure.

## Troubleshooting

- Docker permission error (`permission denied while trying to connect to the docker API`)
  - Ensure Docker Desktop/daemon is running and your user has permission to access Docker.
- `File not found: <video_path>`
  - Confirm the path exists inside the container, not just on the host.
- Network-restricted environment and missing `spdlog`
  - Install `spdlog` in the image or allow network access so CMake can fetch it.
