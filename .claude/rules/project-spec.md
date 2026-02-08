# Video Decoding Benchmark Project Specification

## Project Overview

A benchmark tool to measure how many videos a PC can decode simultaneously in real-time.

### Core Goals
- Measure CPU software decoding performance (excluding GPU hardware acceleration)
- Determine multi-stream concurrent decoding limits
- Support both local files and RTSP network streams (IP cameras, surveillance systems)
- Cross-platform support (Windows, Linux, macOS)
- Distribute as a single executable that is easy for general users to use

---

## Technology Stack

### Language: C++20
- Standard: C++20 or higher (well-supported on all target platforms as of 2026)
- Rich FFmpeg examples and documentation available
- Easy to distribute as a single binary with static linking
- Memory management overhead is minimal for a project of this scale
- Leverage modern C++ idioms and features:
  - RAII and smart pointers (std::unique_ptr with custom deleters) for FFmpeg resource management
  - std::jthread for automatic thread joining and safe lifecycle management
  - std::barrier / std::latch for thread synchronization
  - std::atomic for lock-free shared state
  - std::format for output formatting
  - Structured bindings, std::optional, std::string_view where appropriate
  - Concepts for template constraints if applicable

### Core Library: FFmpeg
- libavcodec: Decoding
- libavformat: Container parsing
- libavutil: Utilities

### Build System: CMake

### Development Environment: Docker
- Use Docker containers for consistent build environments across all platforms
- Development image should include FFmpeg libraries, CMake, and C++ build tools
- Mount the project directory into the container for live editing
- Build and test inside the container to ensure reproducible results

#### Docker Workflow
1. Build the development image: `docker build -t video-bench-dev .`
2. Start the container: `docker run -it -v $(pwd):/app video-bench-dev bash`
3. Build inside the container: `cd /app && mkdir -p build && cd build && cmake .. && make`

### Target OS Requirements
- Ubuntu 24.04 or higher
- Windows 11 or higher
- macOS (latest stable version)

---

## Terminology

### Real-time Decoding
Real-time decoding simulates actual playback by pacing frame decoding to match the target FPS. Each decoder thread:
1. Decodes one frame at a time
2. Waits until the next frame's scheduled time before proceeding
3. If decoding takes longer than the frame interval, the stream is "lagging"

This accurately simulates real-world scenarios like media servers and multi-viewer applications. A stream is considered successful if it maintains the target FPS without excessive lag while keeping average CPU usage ≤ 85%.

---

## Functional Requirements

### Input
- Video source (required): local file path or RTSP URL (e.g., `rtsp://192.168.1.100:554/stream`)
- Maximum number of streams to test (optional, default: CPU thread count)
- Target FPS (optional, default: video's native FPS)

### Behavior
1. Analyze input video information (codec, resolution, FPS)
2. Create N decoder threads (starting from N = 1)
3. Decode the same video in each thread (without rendering)
4. Verify that all streams maintain real-time performance
5. Repeat while incrementing N
6. Report the limit when real-time performance cannot be maintained

### Real-time Performance Criteria
A stream count is considered successful if BOTH conditions are met:
- **FPS Requirement**: Each stream maintains the target FPS or higher
- **CPU Usage Requirement**: Average CPU usage over a 10-second measurement period stays at or below 85%

This ensures the system can handle the workload sustainably without resource exhaustion, while allowing for momentary CPU usage spikes above 85%.

### Example Output

#### Local File
```
CPU: AMD Ryzen 7 5800X (16 threads)
Video: 1080p H.264, 30fps (target: 30fps)

Testing (real-time paced decoding)...
 1 stream:  30.0fps (CPU:  3%) ✓
 2 streams: 30.0fps (CPU:  6%) ✓
 4 streams: 30.0fps (CPU: 12%) ✓
 8 streams: 30.0fps (CPU: 24%) ✓
16 streams: 30.0fps (CPU: 48%) ✓
32 streams: 30.0fps (CPU: 78%) ✓
48 streams: 30.0fps (CPU: 84%) ✓
56 streams: 30.0fps (CPU: 88%) ✗ CPU threshold exceeded
64 streams: 28.5fps (CPU: 92%) ✗ FPS below target

Result: Maximum 48 concurrent streams can be decoded in real-time
```

#### RTSP Stream
```
CPU: AMD Ryzen 7 5800X (16 threads)
Source: rtsp://192.168.1.100:554/stream (live)
Video: 1080p H.264, 30fps (target: 30fps)

Testing (real-time paced decoding)...
 1 stream:  30.0fps (CPU:  4%) ✓
 2 streams: 30.0fps (CPU:  7%) ✓
 4 streams: 30.0fps (CPU: 14%) ✓
 8 streams: 29.8fps (CPU: 28%) ✓

Result: Maximum 8 concurrent RTSP streams can be decoded in real-time
```

---

## Distribution Strategy

### Final Deliverables
```
GitHub Releases:
├── video-benchmark-linux        # Linux x64
├── video-benchmark-windows.exe  # Windows x64
└── video-benchmark-macos        # macOS (Intel/ARM)
```

### User Experience
1. Download the appropriate file for your OS from GitHub Releases
2. Run in terminal
   ```bash
   # Local file
   ./video-benchmark my_video.mp4

   # RTSP stream (IP camera)
   ./video-benchmark rtsp://192.168.1.100:554/stream
   ```
3. View results

---

## Design Guidelines

### Memory Safety
- All FFmpeg resources (AVFormatContext, AVCodecContext, AVFrame, AVPacket, etc.) must be properly freed upon thread completion or error
- Use RAII patterns or custom deleters with smart pointers to guarantee cleanup even on exceptional code paths
- Each decoder thread must own its resources independently — no shared ownership of FFmpeg objects across threads
- Validate all allocations and handle allocation failures gracefully

### Thread Safety & Deadlock Prevention
- Each decoder thread must operate with its own independent FFmpeg context (AVFormatContext, AVCodecContext) — never share these across threads
- Minimize shared mutable state; use thread-local storage where possible
- When shared state is unavoidable (e.g., result aggregation), use a single lock with a consistent acquisition order to prevent deadlocks
- Avoid nested locking — if multiple locks are needed, document and enforce a strict lock ordering
- Prefer lock-free mechanisms (e.g., atomic variables) for simple counters and flags

### Thread Load Balancing
- All decoder threads must start decoding simultaneously using a synchronization barrier (e.g., std::barrier or countdown latch)
- Each thread decodes the same video file independently from the beginning to ensure uniform workload
- Measure per-thread FPS individually and report both per-thread and aggregate metrics
- Monitor for thread starvation — if any single thread's FPS deviates significantly from others, it indicates a scheduling or resource contention issue
- Use OS-level thread affinity hints if needed to ensure fair CPU time distribution across decoder threads

### RTSP Stream Handling
RTSP streams differ from local files in several key aspects:

| Aspect | Local File | RTSP Stream |
|--------|-----------|-------------|
| I/O Source | Disk | Network socket |
| Latency | Predictable | Variable (jitter) |
| Seek | Supported | Not supported (live) |
| Duration | Known | Infinite/unknown |
| Packet Loss | None | Possible |

### Input Source Parity Policy
To keep benchmark results comparable across local files and RTSP streams, the benchmark must model both sources under a real-time arrival constraint.

- The benchmark design must always consider both local file and RTSP execution paths.
- When testing with a local file, packet/frame ingestion must be rate-limited to emulate RTSP-like real-time arrival speed (based on source timestamps / target FPS pacing), not maximum disk read speed.
- File-mode results must be interpreted as "RTSP-like emulated real-time ingest + decode capacity" unless explicitly marked as offline throughput mode.

#### RTSP-specific Requirements
- Use TCP transport (`rtsp_transport=tcp`) for reliability over UDP
- Set connection timeout (default: 5 seconds) to handle unresponsive sources
- Each decoder thread opens its own RTSP connection independently
- No seek-to-start behavior — continuous stream reading
- Handle connection failures gracefully with clear error messages

#### Future RTSP Optimizations (Phase 2+)
- Jitter buffer: 3-5 packet buffer per stream to absorb network latency variation
- Event-driven I/O: Single network thread with epoll/kqueue for 50+ streams
- Connection pooling: Automatic reconnection on stream interruption

---

## Additional Considerations

### Test Codecs
The benchmark tests decoding performance across the following 4 codecs:
| Codec | Description |
|-------|-------------|
| H.264 (AVC) | The most widely used codec globally; baseline for comparison |
| H.265 (HEVC) | ~50% better compression than H.264; standard for 4K/HDR content |
| VP9 | Google's royalty-free codec; widely used on YouTube |
| AV1 | Next-gen royalty-free codec; rapidly growing in streaming (Netflix, YouTube) |

### Test Resolutions
The benchmark tests across 3 resolution tiers to cover low-to-high decoding workloads:
| Label | Resolution | Description |
|-------|------------|-------------|
| HD | 1280x720 (720p) | Low-complexity baseline; common in surveillance and older content |
| FHD | 1920x1080 (1080p) | Most common streaming/conferencing resolution |
| 4K | 3840x2160 (2160p) | High-complexity workload; premium streaming content |

### Test Video Specifications
- Each test video should be prepared for every combination of codec x resolution (4 codecs x 3 resolutions = 12 test files)
- Duration: 30 seconds ~ 1 minute (for repeated decoding)
- Frame rate: 30fps (consistent across all test files)
- Encoding settings: Use standard/default encoding profiles for each codec to reflect real-world usage

### Future Extension Possibilities
- GUI version
- JSON/CSV output for results
- GPU decoding benchmark (NVDEC, VAAPI)
- Real-time CPU/memory usage monitoring
- Advanced RTSP features: jitter buffer, event-driven I/O, auto-reconnection

---

## References

- [FFmpeg Official Documentation](https://ffmpeg.org/documentation.html)
- [FFmpeg Decoding Example](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decode_video.c)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
