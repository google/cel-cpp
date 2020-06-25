FROM gcr.io/cloud-marketplace-containers/google/bazel:3.0.0
RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y tzdata \
    && apt-get autoremove -y \
    && apt-get clean \
    && rm -rf /tmp/* /var/tmp/* \
    && rm -rf /var/lib/apt/lists/*
