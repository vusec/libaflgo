FROM mcr.microsoft.com/devcontainers/base:bionic

RUN apt-get update && \
		apt-get install -y \
				lsb-release \
				wget \ 
				software-properties-common \
				gnupg \
				ninja-build \
				python3-pip

RUN bash -c "$(wget -O - https://apt.kitware.com/kitware-archive.sh)" && \
    apt-get install -y cmake

RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test

RUN wget https://apt.llvm.org/llvm.sh && \
		chmod +x llvm.sh && \
		./llvm.sh 15 && \
		rm llvm.sh

RUN python3 -m pip install \
				lit \
				cmake-format