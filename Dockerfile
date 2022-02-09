FROM gcr.io/gcp-runtimes/ubuntu_20_0_4

ENV DEBIAN_FRONTEND=noninteractive

RUN rm -rf /var/lib/apt/lists/* \
    && apt-get update --fix-missing -qq \
    && apt-get install -qqy --no-install-recommends build-essential ca-certificates tzdata wget git default-jdk clang-12 lld-12 patch \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.5.0/bazelisk-linux-amd64 && chmod +x bazelisk-linux-amd64 && mv bazelisk-linux-amd64 /bin/bazel

ENV CC=clang-12
ENV CXX=clang++-12

RUN mkdir -p /workspace

ENTRYPOINT ["/bin/bazel"]
