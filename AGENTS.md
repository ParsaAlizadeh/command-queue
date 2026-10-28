# Development workflow

- Use `./dev.sh` functions for configuring, building, testing, version detection,
  vendor synchronization, and launching Konsole. See `./dev.sh dev_help`.
- `plugin/` is the plugin source tree. `vendor/` and `build/` are ignored inputs
  and generated outputs; do not commit their contents.
- Keep the user involved in design choices and get agreement before new setup
  changes or feature expansion. Do not provide effort/time estimates.
- Do not install or replace the system Konsole as part of normal development.
