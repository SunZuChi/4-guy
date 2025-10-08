FROM debian:stable-slim

RUN apt-get update && \
    apt-get install -y build-essential g++ cmake git wget && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN g++ -std=c++17 -pthread -O2 -lrt server.cpp -o server
RUN g++ -std=c++17 -pthread -O2 -lrt client.cpp -o client

# default: start server (you can override CMD to start a client)
CMD ["./server"]
