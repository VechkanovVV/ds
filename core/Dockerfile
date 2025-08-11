FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    curl \
    unzip \
    build-essential \
    cmake \
    g++ \
    git \
    pkg-config \
    autoconf \
    automake \
    libtool \
    libssl-dev \
    python3-pip \
    protobuf-compiler \
    libprotobuf-dev \
 && pip3 install conan \
 && conan profile detect --force \
 && apt-get clean && rm -rf /var/lib/apt/lists/*


WORKDIR /app
COPY . /app

RUN protoc --version

RUN chmod +x gen.sh
RUN ./gen.sh

RUN cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=conan/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

CMD ["./build/core"]
