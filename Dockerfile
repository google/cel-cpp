FROM gcc:9

# Install Bazel prerequesites and required tools.
# See https://docs.bazel.build/versions/master/install-ubuntu.html
RUN apt-get update && \
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
ARG BAZEL_VERSION="6.2.0"
ADD https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh /tmp/install_bazel.sh
RUN /bin/bash /tmp/install_bazel.sh && rm /tmp/install_bazel.sh

# When Bazel runs, it downloads some of its own implicit
# dependencies. The following command preloads these dependencies.
# Passing `--distdir=/bazel-distdir` to bazel allows it to use these
# dependencies.  See
# https://docs.bazel.build/versions/master/guide.html#running-bazel-in-an-airgapped-environment
# for more information.
RUN cd /tmp && \
    git clone https://github.com/bazelbuild/bazel && \
    cd bazel && \
    git checkout ${BAZEL_VERSION} && \
    bazel build @additional_distfiles//:archives.tar && \
    mkdir /bazel-distdir && \
    tar xvf bazel-bin/external/additional_distfiles/archives.tar -C /bazel-distdir --strip-components=3 && \
    cd / && \
    rm -rf /tmp/* && \
    rm -rf /root/.cache/bazel

RUN mkdir -p /workspace
RUN mkdir -p /bazel

ENTRYPOINT ["/usr/local/bin/bazel"]
