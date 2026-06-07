ARG BASE_IMAGE_PLATFORM=linux/amd64
FROM --platform=${BASE_IMAGE_PLATFORM} ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

RUN dpkg --add-architecture i386 \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        gcc-multilib \
        g++-multilib \
        gdb \
        gdbserver \
        make \
        cmake \
        valgrind \
        strace \
        ltrace \
        file \
        binutils \
        perl \
        python3 \
        python3-pip \
        python-is-python3 \
        netcat-openbsd \
        curl \
        wget \
        git \
        tar \
        gzip \
        procps \
        iproute2 \
        iputils-ping \
        flex \
        bison \
        tcl-dev \
        tk-dev \
        libx11-dev \
        libc6:i386 \
        libc6-dev-i386 \
        libstdc++6:i386 \
        sudo \
        zlib1g:i386 \
    && groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME} \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
