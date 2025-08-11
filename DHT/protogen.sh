#!/bin/bash

PROTO_DIR="$(dirname "$0")/src/proto"

protoc \
  --proto_path="${PROTO_DIR}" \
  --cpp_out="${PROTO_DIR}" \
  --grpc_out="${PROTO_DIR}" \
  --plugin=protoc-gen-grpc="$(which grpc_cpp_plugin)" \
  "${PROTO_DIR}"/dht.proto