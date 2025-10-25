# http-server
A simple HTTP server and client built in C using sockets

## Overview
This project implements a minimal HTTP server that can serve static HTML files to a client over TCP.  
It includes both a server (listens for requests and serves files) and a `client` (which sends an HTTP GET request and prints the response).

## Prerequisites
* C compiler (e.g., gcc or clang)
* Make

## Install & Run
```bash
git clone https://github.com/chanr335/http-server.git
cd http-server
make
```

Start the server
```bash
./server.c
```

Start the client to request and receive html
```bash
./client <hostname>
```
