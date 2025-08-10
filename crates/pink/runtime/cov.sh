#!/bin/sh
set -e

cargo llvm-cov \
    --html \
    -p pink \
    -p pink-macro \
    -p pink-chain-extension \
    -p pink-runtime \
    -p pink-runtime-macro \
    -p pink-capi \
    -p pink-types \
    -p cyrux-git-revision \
    -p cyrux-serde-more \
    -p cyrux-crypto \
    -p cyrux-mq \
    -p cyrux-sanitized-logger \
    -p cyrux-types \
    -p cyrux-wasm-checker \
    -p reqwest-env-proxy \
