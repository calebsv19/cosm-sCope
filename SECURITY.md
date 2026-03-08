# Security Notes

DataLab is alpha software intended for trusted local workflows.

## Trust Model

- Single-user desktop usage.
- Trusted local `.pack` artifacts.
- Local build and runtime tooling.

## Guidance

- Avoid running with elevated/root privileges.
- Avoid opening untrusted `.pack` files from unknown origins.
- Keep your local environment and dependencies up to date.
- Keep backups of important data sources before running validation workflows.
