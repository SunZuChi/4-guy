FROM debian:stable-slim

# Install compiler and IPC tools
RUN apt-get update && \
    apt-get install -y build-essential g++ cmake git wget util-linux && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

# Build server and client
RUN g++ -std=c++17 -O2 server.cpp -o server -pthread -lrt
RUN g++ -std=c++17 -O2 client.cpp -o client -pthread -lrt

# Default entrypoint: start server
CMD ["./server"]
