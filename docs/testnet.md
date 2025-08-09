Test net
====

### Prepare

- `mkdir ~/cyrux`
- `mkdir ~/cyrux/gk`
- `mkdir ~/cyrux/pruntime_1`

### Build

- `git pull`
- `git submodule update --init`
- `cargo build --release`
- `cd standalone/pruntime/gramine-build && make dist PREFIX=../bin`

### Install

- `cp Workspaces/cyrux-blockchain/target/release/cyrux-node ~/cyrux`

- `cp Workspaces/cyrux-blockchain/target/release/pherry ~/cyrux`
