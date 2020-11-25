FROM ubuntu:bionic

ENV DEBIAN_FRONTEND=noninteractive

RUN rm -rf /var/lib/apt/lists/* \
    && apt-get update --fix-missing -qq \
    && apt-get install -qqy --no-install-recommends ca-certificates tzdata wget git clang-10 patch \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.5.0/bazelisk-linux-amd64 && chmod +x bazelisk-linux-amd64 && mv bazelisk-linux-amd64 /bin/bazel

ENV CC=clang-10
ENV CXX=clang++-10

ENTRYPOINT ["/bin/bazel"]
