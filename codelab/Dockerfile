ARG UBUNTU_IMAGE="ubuntu:22.04"
FROM ${UBUNTU_IMAGE}

ARG BAZELISK_RELEASE="https://github.com/bazelbuild/bazelisk/releases/download/v1.25.0/bazelisk-amd64.deb"

RUN apt update && apt upgrade -y && apt install -y gcc-10 g++-10 clang-14 git curl bash openjdk-11-jdk-headless

RUN curl -L ${BAZELISK_RELEASE} > ./bazelisk.deb
RUN apt install ./bazelisk.deb

RUN git clone https://github.com/google/cel-cpp.git

ENV CXX=g++-10
ENV CC=gcc-10

WORKDIR /cel-cpp
# not generally recommended to cache the bazel build in the image,
# but works ok for prototyping.
RUN bazelisk build ... && bazelisk test //codelab/solutions:all