# Video Benchmark Project

## Coding Rules
- All code, comments, and documentation must be written in English

For detailed project specification, refer to @.claude/rules/project-spec.md

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

## Build & Test Commands
- Docker image build: `docker build -t video-bench-dev -f docker/Dockerfile .`
- Development container: `docker run -it -v $(pwd):/app video-bench-dev bash`
- Build inside container: `mkdir build && cd build && cmake .. && make`
- When building or testing, always use the Docker environment defined in `docker/Dockerfile`
- Do not attempt to build or test on the host machine directly; rely on the Docker container for consistent dependencies

 ## Review workflow
- When user asks for "review", always save the report to:
`reviews/YYYYMMDDHHMMSS-review.md`
- Create `reviews/` if it does not exist.
- Do not modify source code unless explicitly requested.