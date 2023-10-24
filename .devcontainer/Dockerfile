FROM mcr.microsoft.com/devcontainers/base:bionic

RUN apt-get update && \
		apt-get install -y \
				lsb-release \
				wget \ 
				software-properties-common \
				gnupg \
				ninja-build \
				python3-pip

RUN wget -O cmake.tar.gz \
	https://github.com/Kitware/CMake/releases/download/v3.26.4/cmake-3.26.4-linux-x86_64.tar.gz && \
	tar xf cmake.tar.gz -C /usr/local --strip-components=1 && \
	rm cmake.tar.gz

RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test

RUN wget https://apt.llvm.org/llvm.sh && \
		chmod +x llvm.sh && \
		./llvm.sh 15 && \
		rm llvm.sh

RUN python3 -m pip install \
				lit \
				cmake-format