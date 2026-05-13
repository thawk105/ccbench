# syntax=docker/dockerfile:1.4
# Force linux/amd64 because the codebase uses x86-only intrinsics
# (e.g. __cpuid_count in include/cpu.hh). On Apple Silicon this runs
# under QEMU emulation: fine for build/edit/test, not for benchmarking.
#
# Two stages published as two tags:
#   - `base` -> :ci    (minimal — what CI's build.yml actually needs)
#   - `dev`  -> :latest (`base` + human-facing tools used in the
#                       devcontainer: zsh shell, clang-format, sudo,
#                       openssh-client, curl)
#
# Keeping the CI image slim means container init in GitHub Actions
# only has to pull the base layers, which is where the speedup vs the
# old "apt-get install on every run" workflow comes from.
FROM --platform=linux/amd64 ubuntu:24.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Switch to JAIST mirror and keep the apt cache so BuildKit cache mounts work.
RUN sed -i \
        -e 's|http://archive.ubuntu.com/ubuntu/|http://ftp.jaist.ac.jp/pub/Linux/ubuntu/|g' \
        -e 's|http://security.ubuntu.com/ubuntu/|http://ftp.jaist.ac.jp/pub/Linux/ubuntu/|g' \
        /etc/apt/sources.list \
    && rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache

# Build-time deps used by *both* CI and the devcontainer. Don't add
# convenience tools here — those live in the `dev` stage below.
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
        ca-certificates \
        ccache \
        cmake \
        libboost-filesystem-dev \
        libgflags-dev \
        libgoogle-glog-dev

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

# Ubuntu 24.04 ships a default `ubuntu` user at uid=1000, which collides
# with our useradd below. Remove it first; the `|| true` keeps this
# tolerant of older bases that don't have one.
RUN userdel -r ubuntu 2>/dev/null || true \
    && groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME}

WORKDIR /workspaces/ccbench

# CI uses the base stage as-is (root, no extra tooling). It overrides
# WORKDIR and user from the workflow file.


# ---------------------------------------------------------------------------
# `dev` stage: adds the things a human needs but CI doesn't care about.
# Published as :latest and referenced by .devcontainer/devcontainer.json.
# ---------------------------------------------------------------------------
FROM base AS dev

ARG USERNAME=vscode

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
        zsh \
        clang-format \
        sudo \
        openssh-client \
        curl \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}

USER ${USERNAME}
