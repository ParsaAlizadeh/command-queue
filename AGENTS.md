# Development workflow

- Use `./dev.sh` functions for configuring, building, testing, staging, packaging,
  and launching Konsole. See `./dev.sh dev_help`. End users use CMake directly.
- `plugin/` is the plugin source tree. CMake manages the matching Konsole
  checkout and isolated build under `build/_deps/`; `build/` is ignored.
- Keep the user involved in design choices and get agreement before new setup
  changes or feature expansion. Do not provide effort/time estimates.
- Do not install or replace the system Konsole as part of normal development.
  Verify installation with `./dev.sh plugin_stage` instead.
