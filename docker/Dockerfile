# Video Decode Benchmark - Development Environment
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and FFmpeg development libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    python3 \
    python3-tabulate \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Default command
CMD ["/bin/bash"]
