FROM gcc:9

# Install Bazel prerequesites and required tools.
# See https://docs.bazel.build/versions/master/install-ubuntu.html
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      git \
      libssl-dev \
      make \
      pkg-config \
      python3 \
      unzip \
      wget \
      zip \
      zlib1g-dev \
      default-jdk-headless \
      clang-11 && \
    apt-get clean

# Install Bazel.
# https://github.com/bazelbuild/bazel/releases
ARG BAZEL_VERSION="7.2.1"
ADD https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh /tmp/install_bazel.sh
RUN /bin/bash /tmp/install_bazel.sh && rm /tmp/install_bazel.sh

# https://bazel.build/run/build#running-bazel-airgapped
RUN mkdir -p /bazel-distdir && \
    mkdir -p /tmp/empty_workspace && \
    cd /tmp/empty_workspace && \
    touch WORKSPACE && \
    bazel fetch --repository_cache=/bazel-distdir && \
    cd / && \
    rm -rf /tmp/* && \
    rm -rf /root/.cache/bazel

RUN mkdir -p /workspace
RUN mkdir -p /bazel

ENTRYPOINT ["/usr/local/bin/bazel"]
