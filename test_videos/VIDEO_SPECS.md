# Test Video Specifications

## Overview

| Resolution | H.264 | H.265 | VP9 | AV1 |
|------------|-------|-------|-----|-----|
| HD (720p)  | test_video_hd_h264.mp4 | test_video_hd_h265.mp4 | test_video_hd_vp9.webm | test_video_hd_av1.mp4 |
| FHD (1080p) | test_video_fhd_h264.mp4 | test_video_fhd_h265.mp4 | test_video_fhd_vp9.webm | test_video_fhd_av1.mp4 |
| 4K (2160p) | test_video_4k_h264.mp4 | test_video_4k_h265.mp4 | test_video_4k_vp9.webm | test_video_4k_av1.mp4 |

---

## HD (720p) - 1280x720

| File | Codec | Resolution | FPS | Duration | File Size | Bitrate |
|------|-------|------------|-----|----------|-----------|---------|
| test_video_hd_h264.mp4 | H.264 | 1280x720 | 30 | 30.37s | 18 MB | 5.0 Mbps |
| test_video_hd_h265.mp4 | H.265 | 1280x720 | 30 | 30.37s | 11 MB | 3.0 Mbps |
| test_video_hd_vp9.webm | VP9 | 1280x720 | 30 | 30.37s | 29 MB | 8.1 Mbps |
| test_video_hd_av1.mp4 | AV1 | 1280x720 | 30 | 30.37s | 20 MB | 5.5 Mbps |

---

## FHD (1080p) - 1920x1080

| File | Codec | Resolution | FPS | Duration | File Size | Bitrate |
|------|-------|------------|-----|----------|-----------|---------|
| test_video_fhd_h264.mp4 | H.264 | 1920x1080 | 30 | 30.44s | 21 MB | 5.7 Mbps |
| test_video_fhd_h265.mp4 | H.265 | 1920x1080 | 30 | 30.37s | 20 MB | 5.4 Mbps |
| test_video_fhd_vp9.webm | VP9 | 1920x1080 | 30 | 30.37s | 48 MB | 13.3 Mbps |
| test_video_fhd_av1.mp4 | AV1 | 1920x1080 | 30 | 30.37s | 37 MB | 10.3 Mbps |

---

## 4K (2160p) - 3840x2160

| File | Codec | Resolution | FPS | Duration | File Size | Bitrate |
|------|-------|------------|-----|----------|-----------|---------|
| test_video_4k_h264.mp4 | H.264 | 3840x2160 | 30 | 30.37s | 93 MB | 25.7 Mbps |
| test_video_4k_h265.mp4 | H.265 | 3840x2160 | 30 | 30.37s | 42 MB | 11.7 Mbps |
| test_video_4k_vp9.webm | VP9 | 3840x2160 | 30 | 30.37s | 95 MB | 26.4 Mbps |
| test_video_4k_av1.mp4 | AV1 | 3840x2160 | 30 | 30.37s | 74 MB | 20.3 Mbps |

---

## Common Specifications

- **Pixel Format**: yuv420p (all files)
- **Frame Rate**: 30 fps (all files)
- **Duration**: ~30 seconds (all files)
- **Audio**: None (video only)

## Encoding Settings

| Codec | Encoder | Settings |
|-------|---------|----------|
| H.264 | libx264 | preset medium, crf 23 |
| H.265 | libx265 | preset medium, crf 28 |
| VP9 | libvpx-vp9 | crf 30, b:v 0 |
| AV1 | libsvtav1 | crf 30, preset 6 |
