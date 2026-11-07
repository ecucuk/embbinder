---
name: verifying-changes
description: Use when about to report that a change to EmbBinder works, is fixed, or is ready to commit, or when adding logic that needs a test.
---

# Verifying changes in EmbBinder

## Steps

Run the same commands as CI:

```bash
cargo test
cargo clippy --all-targets -- -D warnings
cargo fmt --check
cargo build --no-default-features --target thumbv7em-none-eabihf
cargo run --example basic_service | grep -q "Result = 30"
```

The `no_std` build needs `rustup target add thumbv7em-none-eabihf` once.

## When the change has logic

Add a test to the matching file in `tests/`. For wire-format code, test a truncated message and a `payload_size` larger than the input. Do not change the golden bytes in `tests/wire_format.rs` to make a test pass.

Prove the test bites: temporarily revert the fix, see the test fail, restore.

## Report

State: the test result, clippy result, and the `no_std` build result.
