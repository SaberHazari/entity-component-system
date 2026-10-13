# ECS — Memory Foundation

A small game-engine project built around an Entity Component System.
This repo currently contains only the memory layer that will support the ECS;
the ECS itself is not written yet.

## What's here

A dynamically growing **arena allocator** with a **permanent / transient**
storage split, plus a thin platform layer that backs the arenas with the OS
allocator.

### Arena allocator

- Bump allocation over a linked list of blocks.
- Blocks grow on demand, the arena asks the platform allocator for a new
  block when the current one can't satisfy a push. Sizing is controlled by a
  per-arena `minimum_block_size` so syscalls stay rare.
- Any power of two alignment is supported, on both the current-block and
  fresh-block paths.
- `arena_mark` / `arena_pop_to` give O(k)-in-blocks-freed scoped rollback.
  Popping frees whole blocks back to the platform and truncates the marked
  block in place.
- `arena_clear` keeps the first block alive and resets its used count, the
  cheap per-frame reset path.
- Allocations are zeroed by default through `arena_new_m` / `arena_array_m`.
  The raw `arena_push` skips zeroing for callers that overwrite immediately.
- ASAN poisoning of freed regions when built with `-fsanitize=address`.
- Allocation failure calls an optional handler, then `abort()`.

### Permanent / transient storage

`Memory` bundles two arenas with different reset policies:

| | Permanent | Transient |
|---|---|---|
| Lifetime | Whole session | One frame (or one `TempMemory` scope) |
| Reset | Never during normal gameplay | `memory_frame_reset` each frame |
| Intended for | Game state, world, save-relevant data | Per-frame scratch, render lists, sim math |

`TempMemory` wraps `arena_mark` / `arena_pop_to` for nested scoped scratch
inside the transient arena.

### Platform layer

The game never calls the OS allocator directly. The platform supplies
`ArenaAllocFn` / `ArenaFreeFn` at `memory_init` time. The current `main.c`
provides an `mmap` / `mprotect` wrapper for Linux/macOS; the same shape plugs
into `VirtualAlloc` / `VirtualFree` on Windows.

## Design notes

This allocator is closely modeled on the one Casey Muratori built during
Handmade Hero, that series was one of my learning materials, and
the permanent/transient split with a linked-block arena is, in my opinion, a
genuinely good way to structure a game's memory. Robustness hasn't been
exhaustively proven yet; more testing is needed before I trust it under
sustained load.

The ECS that will sit on top of it is planned to diverge from Casey's
approach, it will be traditional **struct-of-arrays** component storage rather 
than the more experimental layouts he explored.