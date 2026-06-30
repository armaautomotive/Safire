#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="${TMPDIR:-/tmp}/safire-double-spend-tests"

mkdir -p "$OUT_DIR"
cd "$PROJECT_ROOT"

NICE_CFLAGS=($(pkg-config --cflags nice))
NICE_LIBS=($(pkg-config --libs nice))

if [[ "${OSTYPE:-}" == darwin* ]]; then
  clang++ -arch x86_64 -std=c++11 -stdlib=libc++ -w \
    "${NICE_CFLAGS[@]}" \
    -I./src -I/usr/local/opt/openssl/include -I/usr/local/include -I/usr/local/Cellar/boost/1.62.0/include -I./src/leveldb/include \
    ./src/tests/double_spend_tests.cpp \
    ./src/platform.cpp ./src/wallet.cpp ./src/ecdsacrypto.cpp ./src/blockdb.cpp ./src/userdb.cpp ./src/networktime.cpp ./src/log.cpp \
    ./src/functions/functions.cpp ./src/functions/chain.cpp ./src/functions/chainvalidator.cpp ./src/functions/ledgerstate.cpp ./src/functions/selector.cpp ./src/functions/validation.cpp \
    ./src/compression.cpp ./src/base64.cpp \
    -L/usr/local/opt/openssl/lib -L/usr/local/lib /usr/local/opt/leveldb/lib/libleveldb.a \
    -lssl -lcrypto -lboost_system -lboost_thread-mt -lboost_filesystem -lsnappy -lcurl -lz "${NICE_LIBS[@]}" \
    -o "$OUT_DIR/double_spend_tests"
else
  "${CXX:-g++}" -pthread -Wdeprecated -w "${NICE_CFLAGS[@]}" -std=c++11 \
    -I./src -I/usr/include/openssl -I/usr/local/include -I./src/leveldb/include \
    ./src/tests/double_spend_tests.cpp \
    ./src/platform.cpp ./src/wallet.cpp ./src/ecdsacrypto.cpp ./src/blockdb.cpp ./src/userdb.cpp ./src/networktime.cpp ./src/log.cpp \
    ./src/functions/functions.cpp ./src/functions/chain.cpp ./src/functions/chainvalidator.cpp ./src/functions/ledgerstate.cpp ./src/functions/selector.cpp ./src/functions/validation.cpp \
    ./src/compression.cpp ./src/base64.cpp \
    -L/usr/local/lib/ -L./src/leveldb -L/usr/lib/x86_64-linux-gnu/ -pthread \
    -lboost_system -lboost_thread -lssl -lcrypto -lboost_filesystem -lleveldb -lcurl -lz "${NICE_LIBS[@]}" \
    -o "$OUT_DIR/double_spend_tests"
fi

cd "$OUT_DIR"
./double_spend_tests
