# Cyrux Blockchain

Cyrux Network is the offchain computing protocol, powering the decentralized execution layer for AI agents.

## Native Build

### Dependencies

<details><summary>Expand</summary>

- System dependencies
  - Ubuntu (tested with 22.04)
  ```bash
  apt install -y build-essential pkg-config libssl-dev protobuf-compiler
  ```

  - macOS
  ```bash
  brew install protobuf
  ```

  - See [here](https://grpc.io/docs/protoc-installation/) for more protobuf installation options

- Rustw


  ```bash
  curl https://sh.rustup.rs -sSf | sh
  ```

- Substrate dependencies:

   ```bash
   git submodule update --init
   sh ./scripts/init.sh
   ```

- LLVM 14

  ```bash
  wget https://apt.llvm.org/llvm.sh
  chmod +x llvm.sh
  ./llvm.sh 14
  ```

</details>

### Build the blockchain and bridge

Make sure you have Rust and LLVM-10 installed.

> Note for Mac users: you also need `llvm` and `binutils` from Homebrew or MacPort, and to add their binaries to your $PATH

Run `git submodule update --init` to fetch submodules before build if you haven't add option `--recursive ` when clone code.

```bash
cargo build --release
```

The build script enforces LLVM-10 or newer is used. LLVM-10 is needed because of the wasm port of rust
crypto library, `ring`. We have to compile the C code into wasm while keeping the compatibility with
the _current_ rustc.

## Run

Please refer to [the run scripts](./scripts/run)

## Sub-pages

- [RPC](./docs/): RPC documentations
- [Test](./docs/test.md): How to test the components

## External Resources

- [Docs](): Cyrux Network Documentations (coming soon).
- [cyrux-docker](https://github.com/Cyrux-Network): The production dockerfiles, including the blockchain, pherry, and pRuntime (coming soon).
- [Responsible Disclosure](./docs/responsible-disclosure.md)


## Security Notice

For cybersecurity and operational consistency, all contributions to this repository are uploaded using a shared developer account.  
This approach helps ensure a unified audit trail and reduces the exposure of individual developer credentials.