# C++ File Size Guidelines

## Line Count Targets

| File Type | Ideal | Soft Limit | Hard Limit |
|-----------|-------|------------|------------|
| Header (.h/.hpp) | 100–300 lines | 500 lines | 800 lines |
| Source (.cpp) | 200–500 lines | 1,000 lines | 1,500 lines |
| Function body | 20–50 lines | 80 lines | 150 lines |

## Rules

1. **Default behavior**: Keep files within the "Ideal" range whenever possible.
2. **Soft limit**: May be exceeded when a single cohesive responsibility genuinely requires more code. Add a brief comment at the top of the file explaining why it is large.
3. **Hard limit**: Only exceeded in truly exceptional cases (e.g., complex state machines, protocol parsers, generated code). Must be justified and documented.
4. **Splitting preference**: When a file grows beyond the soft limit, prefer splitting by logical responsibility rather than arbitrary line count.

## When Long Files Are Acceptable

- A single algorithm or state machine that loses clarity when split across files
- Tightly coupled logic where splitting would increase complexity instead of reducing it
- Auto-generated or table-driven code (lookup tables, codec mappings, etc.)

## When to Split

- The file handles more than one distinct responsibility
- You find yourself scrolling extensively to navigate between unrelated sections
- Multiple developers frequently need to edit the same file for unrelated changes
- The file contains utility functions that could live in a shared module

## Header File Specifics

- One primary class per header (closely related helper classes/structs are fine)
- Prefer forward declarations over includes to keep headers lean
- Move inline implementations to .cpp unless performance-critical (templates excepted)

## Function Size

- A function should ideally fit on one screen (~50 lines)
- If a function exceeds 80 lines, consider extracting helper functions
- Long functions are acceptable only when linear step-by-step logic would become harder to follow if split
