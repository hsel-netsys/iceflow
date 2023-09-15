FROM ghcr.io/hsel-netsys/iceflow-ci-image:main
COPY . /
RUN cmake . && \
    make

ENTRYPOINT [ "/bin/bash" ]
