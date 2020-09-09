FROM ubuntu:bionic

RUN rm -rf /var/lib/apt/lists/* \
    && apt-get update --fix-missing -qq \
    && apt-get install -qqy --no-install-recommends ca-certificates tzdata wget \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.5.0/bazelisk-linux-amd64 && chmod +x bazelisk-linux-amd64 && mv bazelisk-linux-amd64 /bin/bazel

ENTRYPOINT ["/bin/bazel"]
