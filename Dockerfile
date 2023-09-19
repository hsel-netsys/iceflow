FROM ubuntu:latest
COPY . /
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get -y install git cmake wget software-properties-common build-essential pkg-config python3-minimal libssl-dev libsqlite3-dev libopencv-dev clang-format libboost-all-dev libpcap-dev libsystemd-dev psmisc sudo && \
    ln -s /usr/include/opencv4 /usr/local/include/opencv4 && \
    # Install nlohmann-json
    add-apt-repository ppa:team-xbmc/ppa &&  \
    apt-get update && \
    apt-get -y install nlohmann-json3-dev && \
    # Install ndn-cxx
    git clone https://github.com/named-data/ndn-cxx && \
    cd ndn-cxx && \
    ./waf configure && \
    ./waf && \
    ./waf install && \
    ldconfig && \
    cd .. && \
    rm -rf ndn-cxx && \
    # Install PSync
    git clone https://github.com/named-data/PSync.git && \
    cd PSync && \
    ./waf configure && \
    ./waf && \
    ./waf install && \
    cd .. && \
    rm -rf PSync && \
    # Install yaml-cpp
    git clone https://github.com/jbeder/yaml-cpp.git && \
    cd yaml-cpp && \
    mkdir build && \
    cd build && \
    cmake -DYAML_BUILD_SHARED_LIBS=on .. && \
    make && \
    make install && \
    cd ../.. && \
    rm -rf yaml-cpp && \
    # Build and install NFD
    git clone --recursive https://github.com/JKRhb/NFD-out-of-the-box.git NFD --branch self-learning-rebased && \
    cd NFD && \
    ./waf configure && \
    ./waf && \
    ./waf install && \
    ldconfig && \
    cp /usr/local/etc/ndn/nfd.conf.sample /usr/local/etc/ndn/nfd.conf && \
    cd .. && \
    rm -rf NFD && \
    # Build and install NDN tools
    git clone https://github.com/named-data/ndn-tools.git && \
    cd ndn-tools && \
    ./waf configure && \
    ./waf && \
    ./waf install && \
    cd .. && \
    rm -rf ndn-tools && \
    # Build IceFlow
    cmake .  && \
    make

ENTRYPOINT [ "/bin/bash" ]
