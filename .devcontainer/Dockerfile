# syntax=docker/dockerfile:1.4
# Force linux/amd64 because the codebase uses x86-only intrinsics
# (e.g. __cpuid_count in include/cpu.hh). On Apple Silicon this runs
# under QEMU emulation: fine for build/edit/test, not for benchmarking.
FROM --platform=linux/amd64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Switch to JAIST mirror and keep the apt cache so BuildKit cache mounts work.
RUN sed -i \
        -e 's|http://archive.ubuntu.com/ubuntu/|http://ftp.jaist.ac.jp/pub/Linux/ubuntu/|g' \
        -e 's|http://security.ubuntu.com/ubuntu/|http://ftp.jaist.ac.jp/pub/Linux/ubuntu/|g' \
        /etc/apt/sources.list \
    && rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache

# Install build dependencies. Under QEMU emulation the bottleneck is dpkg
# post-install scripts, not download speed, so apt-fast doesn't help here.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        autoconf \
        automake \
        libtool \
        pkg-config \
        git \
        openssh-client \
        ca-certificates \
        sudo \
        curl \
        clang-format \
        cmake \
        libboost-filesystem-dev \
        libgflags-dev \
        libgoogle-glog-dev \
        zsh

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}

USER ${USERNAME}
WORKDIR /workspaces/ccbench
