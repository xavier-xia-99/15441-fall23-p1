# 15-441/15-641 Project 1: Mixnet

Project 1 for the 15-441/641 (Networking and the Internet) course at CMU. **Note**: This is a course project, so DO NOT push your solution to a public fork of this repo.

## Requirements

This project is designed to run on Linux. It has been tested on Ubuntu 20.04.2+, but you may get away with using other distributions.

## Building

You need `cmake` to build the project. On Ubuntu, you can install this with:
```bash
sudo apt install build-essential cmake
```

To build the project, in the root directory, run:
```bash
mkdir build
cd build
cmake ..
make
```

From now on you can always build the project by going to the `build` directory and running `make`.

## Running

Mixnet is test-driven. You can find some examples of these tests under the `testing` directory. For instance, [cp1/testcase_line_easy.cpp](testing/cp1/testcase_line_easy.cpp) demonstrates how to create a line topology with two nodes, 'subscribe' to packet data from both of them, and send a FLOOD packet from one to the other.

You can run mixnet in two modes: *autotester* mode, which we will be using to grade your implementation, and *manual* mode, which you can use to debug your implementation on either a single machine (using one process per mixnet node) or a cluster of machines. You will also use the manual mode to perform experiments on AWS (please see the handout for details).

To run in autotester mode, `cd` into the build directory and run:
```
./bin/cp1/testcase_line_easy -a # '-a' toggles the autotester
```
At the end, it should produce output indicating whether your implementation passed or failed that particular test-case.

You can also run the same test in 'manual' mode. For the `testcase_line_easy` example, you will need three terminal windows open: one for each of the two mixnet nodes, and one for the 'orchestrator', which bootstraps the topology, sets up connections, coordinates actions, etc. In general, you will need (n + 1) terminals, where n is the number of mixnet nodes in the test topology. First, start the orchestrator:
```
./bin/cp1/testcase_line_easy # Note that '-a' is missing
```
You should see output that looks like this: ```[Orchestrator] Started listening on port 9107```. Note the port (it's always 9107) the orchestrator is running on. Next, type the following commands in the other two terminals (one in each):
```
./bin/node 127.0.0.1 9107
./bin/node 127.0.0.1 9107
```
The format is as follows: `./node {server_ip} {server_port}` (also see `./bin/node -h`). The `{server_ip}` argument corresponds to the *public* IPv4 address of the machine on which the orchestrator is running; since we are running everything locally, we can simply use the machine's loopback address (127.0.0.1). Please refer to the other test-cases, as well as the test API in [framework/orchestrator.h](framework/orchestrator.h#L174) for more examples and detailed usage.

The entry-point to your code is the `run_node()` function in [mixnet/node.c](mixnet/node.c). For details, please refer to the handout. Good luck!
