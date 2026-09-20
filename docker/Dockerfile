FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake curl git libboost-dev libboost-system-dev \
    libfastcdr-dev netcat-openbsd ninja-build nlohmann-json3-dev pkg-config python3 python3-pip \
 && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages --no-cache-dir \
    'jsonschema>=4.23.0' 'pydantic>=2.7.0'

RUN curl -fsSL https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain 1.97.1
ENV RUSTUP_TOOLCHAIN="1.97.1"
ENV PATH="/root/.cargo/bin:/opt/hakoniwa/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/hakoniwa/lib"

WORKDIR /workspace
