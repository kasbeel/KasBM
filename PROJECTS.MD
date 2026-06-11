# KasBM Project Overview

## Objective

KasBM aims to be a minimalistic build manager written in C++ for small to medium native projects. The current implementation focuses on a small command surface, JSON-based project description, dependency-aware target builds, and fast incremental recompilation without introducing a large toolchain layer.

The practical direction of the project today is:

- keep the build flow simple and transparent
- describe projects in `kas_package.json`
- support multiple build targets with explicit dependencies
- integrate with system libraries through `pkg-config`
- keep the implementation lightweight and easy to audit

## Current Feature Set

### CLI Commands

KasBM currently exposes these commands:

- `kasbm build` builds all configured targets in dependency order
- `kasbm build -t <target>` builds a single target and its declared target dependencies
- `kasbm run <command>` executes a named command from `kas_package.json` when that command is enabled
- `kasbm help` prints the built-in CLI help
- `kasbm clean` exists in the CLI, but currently only prints a success message and does not remove artifacts

### Project Configuration

The tool reads a `kas_package.json` file from the current working directory and deserializes the following main sections:

- project metadata: schema version, project name, version, description, and license
- toolchain configuration: compiler, language standards, warning settings, job count, sysroot, compiler flags, and linker flags
- profiles: named profile entries with defines, extra compiler flags, LTO, and strip settings
- targets: named build targets with type, sources, include paths, system libraries, internal target dependencies, output naming, build directories, and extra flags
- commands: named shell commands that can be executed through `kasbm run <name>`

### Build Pipeline

The current build pipeline implements the following behavior:

- resolves target-to-target dependencies before building
- supports building all targets or a single requested target
- discovers source files from simple filesystem patterns
- compiles source files in parallel using a semaphore-based job limiter
- chooses the number of compile jobs from `toolchain.jobs`, or falls back to detected hardware concurrency
- performs incremental recompilation based on object timestamps
- also checks generated dependency files (`.d`) to rebuild when included project headers change
- automatically enables dependency file generation with `-MMD` and `-MP`
- creates object file directories automatically
- links finished objects into the target output

### Supported Target Behavior

The code currently handles these target behaviors:

- `executable` targets link into an executable placed in `output_dir` or `bin` by default
- `shared_lib` targets are compiled with `-fPIC`, linked with `-shared`, versioned using the project version, and accompanied by an unversioned symlink

The schema also accepts `static_lib`, but static archive creation is not fully implemented in the current linker path.

### Dependency and System Library Handling

KasBM currently supports:

- checking required system libraries with `pkg-config --exists`
- collecting compile flags from `pkg-config --cflags`
- collecting linker flags from `pkg-config --libs`
- aborting the build when a required system library is missing
- adding shared-library target dependencies to the linker search path and link flags

This makes the current implementation most suitable for Unix-like environments where `pkg-config`, shell commands, and symlink creation are available.

### File Pattern Resolution

The filesystem layer currently resolves a limited and explicit pattern model:

- `path/**` for recursive discovery
- `path/*` for non-recursive discovery
- direct paths for individual files or directories

For source discovery, the implementation filters for `.cpp` files. For include directory discovery, the same pattern mechanism can return directories.

### Execution and Logging

KasBM includes:

- structured log levels: `DEBUG`, `INFO`, `WARN`, and `ERROR`
- colored terminal output when stdout is attached to a TTY
- shell-based execution for configured runtime commands

## Implemented but Not Fully Wired

Several configuration fields are already parsed but are not yet applied in the build path:

- `profiles` are parsed from JSON, and the CLI accepts `--profile`, but profile flags, defines, LTO, and strip settings are not currently injected into compilation or linking
- `--verbose` is parsed, but it does not currently change log verbosity or command output behavior
- toolchain warning settings are parsed, but they are not translated into compiler flags
- `sysroot` is parsed, but it is not used in compiler or linker invocation

## Known Limitations in the Current Code

- `kasbm clean` does not delete build outputs yet
- `static_lib` is recognized by configuration parsing, but the linker path does not build an actual `.a` archive
- command execution relies on shell commands through `system()`, so behavior depends on the host environment
- missing dependency suggestions are currently Debian-style (`lib<name>-dev`) and are not distribution-aware
- the sample `kas_package.json` uses `out_dir`, while the implementation reads `output_dir`; `out_dir` is currently ignored

## What KasBM Already Does Well

In its current state, KasBM already provides a usable core for a minimal C++ build manager:

- one small executable with a narrow command set
- straightforward JSON project description
- explicit multi-target build ordering
- parallel compilation
- incremental rebuilds with header dependency tracking
- direct integration with system packages through `pkg-config`
- support for custom runnable project commands

## Recommended Short-Term Direction

To stay aligned with the goal of being a minimalistic C++ build manager, the next steps should prioritize depth over surface area:

1. Make `--profile` real by applying defines, compiler flags, LTO, and strip behavior.
2. Implement actual cleaning of target build and output directories.
3. Finish `static_lib` support with archive generation.
4. Apply warning and sysroot settings in the compiler and linker commands.
5. Normalize configuration keys so the documented schema and shipped sample file use the same field names.

## Summary

KasBM is already a working minimal build tool with dependency-aware target builds, parallel compilation, incremental rebuild support, and `pkg-config` integration. Its current gap is not the absence of a core engine, but the incomplete wiring of several configuration features that are already present in the schema and CLI surface.