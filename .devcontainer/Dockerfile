FROM mcr.microsoft.com/devcontainers/base:jammy

RUN apt-get update && \
		apt-get install -y \
				lsb-release \
				wget \ 
				software-properties-common \
				gnupg \
				cmake \
				ninja-build \
				python3-pip

RUN wget https://apt.llvm.org/llvm.sh && \
		chmod +x llvm.sh && \
		./llvm.sh 16 all && \
		rm llvm.sh

RUN python3 -m pip install \
				lit==16