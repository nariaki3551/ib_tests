FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    libibverbs-dev \
    librdmacm-dev \
    ibverbs-utils \
    libmlx5-1 \
    openmpi-bin \
    libopenmpi-dev \
    pkg-config \
    hwloc \
    numactl \
    vim \
    bash-completion \
    && rm -rf /var/lib/apt/lists/*

# Add bash completion for make
RUN echo "\
if [ -f /usr/share/bash-completion/bash_completion ]; then\n\
  . /usr/share/bash-completion/bash_completion\n\
fi\n\
\n\
_makefile_targets() {\n\
  COMPREPLY=(\$(compgen -W \"\$(make -qp | awk -F':' '/^[a-zA-Z0-9][^\\#\\t=]*:([^=]|$)/ {print \$1}' | sort -u)\" -- \"\${COMP_WORDS[COMP_CWORD]}\"))\n\
}\n\
complete -F _makefile_targets make\n\
" >> /etc/bash.bashrc

WORKDIR /app
COPY container/ .

CMD ["/bin/bash"]
