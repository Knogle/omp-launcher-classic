# Contributing

## Scope

- Keep changes focused and reviewable.
- Prefer small pull requests over large mixed refactors.
- Do not introduce bundled proprietary payloads such as `omp-client.dll`.

## Development

Primary targets:

- Windows 64-bit for release builds
- Linux or toolbox-based environments for local Rust quality checks

Recommended local checks:

```bash
cargo fmt --check --manifest-path inject_helper/Cargo.toml
cargo clippy --manifest-path inject_helper/Cargo.toml --all-targets -- -D warnings
```

Windows packaging in the `devbuild` toolbox:

```bash
./local/build_omp_launcher_classic_windows_in_devbuild.sh
./local/build_omp_launcher_classic_setup_windows_in_devbuild.sh
```

## Pull Requests

- Update documentation when behavior, build steps, or packaging changes.
- Describe user-visible changes and testing performed.
- Keep security-sensitive changes explicit in the PR description.

## Security

- Do not add cleartext secrets to committed files, screenshots, or issue reports.
- Report vulnerabilities privately as described in [SECURITY.md](./SECURITY.md).
