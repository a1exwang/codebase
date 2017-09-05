FROM ubuntu:17.04
LABEL maintainer="a1exwang <ice_b0und@hotmail.com>"

RUN apt-get update
RUN apt-get install -y gcc g++ cmake

RUN mkdir -p /data/build

ADD os /data/os
ADD db /data/db
ADD network /data/network
ADD CMakeLists.txt /data/CMakeLists.txt
WORKDIR /data
RUN cd /data/build && cmake ..
RUN cd /data/build && make -j8
