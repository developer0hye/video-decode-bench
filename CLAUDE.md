# Video Benchmark Project

## Coding Rules
- All code, comments, and documentation must be written in English
- When working on throughput improvements, always design changes to benefit both local file and RTSP input paths — avoid optimizations that only help one case at the expense of the other

For detailed project specification, refer to @.claude/rules/project-spec.md

## Commit Rules
- All commits must include a `Signed-off-by` line to pass DCO check (always use `git commit -s`)
- All commits must use the local git config `user.name` and `user.email` for both author and committer
- The `Signed-off-by` name must match the commit author identity

## Branch Rules
- Always create a new branch before starting work on a new feature or task
- Branch naming convention: `feature/<short-description>` (e.g., `feature/rtsp-support`, `feature/csv-export`)
- Never commit new feature work directly to the `main` branch
- Never use `git checkout` or `git switch` to change branches. Use `git worktree` to work on multiple branches simultaneously in separate directories

### Git Worktree Workflow

1. **Create a worktree for a new feature:**
   ```bash
   git worktree add ../video-decode-bench-<branch-name> -b feature/<short-description>
   ```
   Example:
   ```bash
   git worktree add ../video-decode-bench-rtsp-support -b feature/rtsp-support
   ```

2. **Work inside the worktree directory**, not the main repository:
   ```bash
   cd ../video-decode-bench-rtsp-support
   ```

3. **Cleanup after merging:** Remove the worktree when the branch is merged:
   ```bash
   git worktree remove ../video-decode-bench-<branch-name>
   ```

4. **List active worktrees:**
   ```bash
   git worktree list
   ```

## Task Execution Guidelines
- Break down all tasks into extremely small, atomic units
- Each unit should be independently verifiable and testable
- Execute tasks one step at a time in sequential order
- Verify completion of each step before proceeding to the next
- Document progress after each micro-step completion
- Never skip or combine steps for efficiency
- If a step seems complex, break it down further into smaller sub-steps

## Logging Rules
- Logging must be minimal: only include logs that help diagnose program issues or clarify expected output for the user
- Avoid verbose or redundant log messages; each log line should serve a clear diagnostic or informational purpose
- Use appropriate log levels (e.g., ERROR for failures, WARN for potential issues, INFO for key execution milestones, DEBUG for development-only details)
- Never log sensitive data or large data dumps; keep messages concise and actionable

## Program Execution Rules
- Always run programs sequentially — never run multiple program executions in parallel
- Wait for the current execution to fully complete before starting the next one
- This applies to all program runs including builds, tests, benchmarks, and A/B tests

## Build & Test Commands
- Docker image build: `docker build -t video-bench-dev -f docker/Dockerfile .`
- Development container: `docker run -it -v $(pwd):/app video-bench-dev bash`
- Build inside container: `mkdir build && cd build && cmake .. && make`
- When building or testing, always use the Docker environment defined in `docker/Dockerfile`
- Do not attempt to build or test on the host machine directly; rely on the Docker container for consistent dependencies
- When running video decoding tests, always use `--max-streams 1024`

## Docker Execution Rules
- Never use `cd <dir> && docker ...` pattern — always start commands with `docker` directly
- Use absolute paths in `-v` mounts instead of `$(pwd)` (required for `Bash(docker *)` permission pattern to match)
  - ✅ `docker run --rm -v /absolute/path/to/worktree:/app video-bench-dev bash -c "..."`
  - ❌ `cd /some/worktree && docker run --rm -v "$(pwd)":/app video-bench-dev bash -c "..."`
- This is especially important with git worktree workflows where mount paths vary per branch

## A/B Testing Guidelines

When comparing performance before/after code changes, use consistent log file naming:

### Log File Naming Convention
```
logs/<YYYYMMDDHHMMSS>-<test-name>-<variant>.log
```

Examples:
- `logs/20260208143000-thread-optimization-before.log`
- `logs/20260208143000-thread-optimization-after.log`
- `logs/20260208150500-atomic-batch-baseline.log`
- `logs/20260208150500-atomic-batch-optimized.log`

### A/B Test Workflow

1. **Create logs directory** (if not exists):
   ```bash
   mkdir -p logs
   ```

2. **Test BEFORE changes** (baseline) — use the main worktree:
   ```bash
   # Build and run in the main worktree (unchanged code)
   docker run --rm -v /path/to/video-decode-bench:/app video-bench-dev bash -c \
     "cd /app/build && cmake .. && make -j4"
   docker run --rm -v /path/to/video-decode-bench:/app video-bench-dev bash -c \
     "/app/build/video-benchmark /app/test_videos/test_video_hd_h264.mp4 \
       --max-streams 32 --log-file /app/logs/20260208143000-optimization-before.log"
   ```

3. **Test AFTER changes** (optimized) — use the feature worktree:
   ```bash
   # Build and run in the feature worktree (modified code)
   docker run --rm -v /path/to/video-decode-bench-<branch-name>:/app video-bench-dev bash -c \
     "cd /app/build && cmake .. && make -j4"
   docker run --rm -v /path/to/video-decode-bench-<branch-name>:/app video-bench-dev bash -c \
     "/app/build/video-benchmark /app/test_videos/test_video_hd_h264.mp4 \
       --max-streams 32 --log-file /app/logs/20260208143000-optimization-after.log"
   ```

4. **Compare results**: Check max streams, CPU usage, and FPS stability

### Recommended Test Matrix
For comprehensive A/B testing, run these combinations:
- **Resolutions**: HD (720p), FHD (1080p), 4K
- **Codecs**: H.264, H.265
- **Stream counts**: --max-streams 16, 32, 64

### Log File CLI Option
```bash
./video-benchmark --log-file <path> <video_source>
./video-benchmark -l <path> <video_source>
```

## Pre-Commit Rules
- Before committing, if any `.cpp`, `.hpp`, `.h`, or `.c` files have been modified, you MUST perform an A/B test comparing before and after the changes
- Do not commit code changes without verifying that the A/B test results show no regression (or show expected improvement)
- Follow the A/B Testing Guidelines above for the test procedure

## Review workflow
- When user asks for "review", always save the report to:
`reviews/YYYYMMDDHHMMSS-<title>-review.md`
  - `<title>`: lowercase kebab-case summary of the review topic (e.g., `thread-pool-refactor`, `memory-leak-fix`, `rtsp-streaming`)
  - Example: `reviews/20260208215558-thread-pool-refactor-review.md`
- Create `reviews/` if it does not exist.
- Do not modify source code unless explicitly requested.