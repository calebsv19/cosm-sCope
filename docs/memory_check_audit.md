# DataLab Memory-Check Audit

`datalab` provides a default-off fisiCs memory-check lane for the pack-loader
allocation path:

```sh
make -C datalab memory-check-audit
```

The audit rebuilds the existing pack-loader test with the
`physics-units,memory-check` overlay, links the fisiCs memory-check runtime,
and runs the generated focused test binary. This keeps the diagnostic pass
separate from the normal Clang build, desktop packaging, and release flow.

Report files:

- `datalab/build/memory_check/datalab.stdout`
- `datalab/build/memory_check/datalab.stderr`

## Current Baseline

Last audited: 2026-06-07

```text
[fisics:memory-check] summary: active=0 leaked_bytes=0 allocs=5 frees=5 double_free=0 unknown_free=0 tracker_failures=0
```
