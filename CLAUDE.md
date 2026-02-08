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

## Build Commands
- Development: `docker run -it -v $(pwd):/app video-bench-dev bash`
- Build: `mkdir build && cd build && cmake .. && make`

 ## Review workflow
- When user asks for "review", always save the report to:
`reviews/YYYYMMDDHHMMSS-review.md`
- Create `reviews/` if it does not exist.
- Do not modify source code unless explicitly requested.