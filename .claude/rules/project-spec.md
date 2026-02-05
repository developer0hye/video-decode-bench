# Video Decoding Benchmark Project Specification

## Project Overview

A benchmark tool to measure how many videos a PC can decode simultaneously in real-time.

### Core Goals
- Measure CPU software decoding performance (excluding GPU hardware acceleration)
- Determine multi-stream concurrent decoding limits
- Cross-platform support (Windows, Linux, macOS)
- Distribute as a single executable that is easy for general users to use

---

## Technology Stack

### Language: C++
- Rich FFmpeg examples and documentation available
- Easy to distribute as a single binary with static linking
- Memory management overhead is minimal for a project of this scale

### Core Library: FFmpeg
- libavcodec: Decoding
- libavformat: Container parsing
- libavutil: Utilities

### Build System: CMake

### Target OS Requirements
- Ubuntu 24.04 or higher
- Windows 11 or higher
- macOS (latest stable version)

---

## Functional Requirements

### Input
- Video file path (required)
- Maximum number of streams to test (optional, default: CPU thread count)
- Target FPS (optional, default: video's native FPS)

### Behavior
1. Analyze input video information (codec, resolution, FPS)
2. Create N decoder threads (starting from N = 1)
3. Decode the same video in each thread (without rendering)
4. Verify that all streams maintain real-time (target FPS) or higher
5. Repeat while incrementing N
6. Report the limit when real-time cannot be maintained

### Example Output
```
CPU: AMD Ryzen 7 5800X (16 threads)
Video: 1080p H.264, 30fps

Testing...
 1 stream:  847fps ✓
 2 streams: 423fps ✓
 4 streams: 211fps ✓
 8 streams: 105fps ✓
12 streams:  68fps ✓
16 streams:  49fps ✓
20 streams:  38fps ✓
24 streams:  29fps ✗

Result: Maximum 20 concurrent streams can be decoded (real-time)
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
   ./video-benchmark my_video.mp4
   ```
3. View results

---

## Additional Considerations

### Supported Codecs
- H.264 (AVC) - Most common
- H.265 (HEVC)
- VP9
- AV1

### Recommended Test Video Specifications
- Resolution: 1080p or 4K
- Codec: H.264
- Duration: 30 seconds ~ 1 minute (for repeated decoding)

### Future Extension Possibilities
- GUI version
- JSON/CSV output for results
- GPU decoding benchmark (NVDEC, VAAPI)
- Real-time CPU/memory usage monitoring

---

## References

- [FFmpeg Official Documentation](https://ffmpeg.org/documentation.html)
- [FFmpeg Decoding Example](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decode_video.c)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
