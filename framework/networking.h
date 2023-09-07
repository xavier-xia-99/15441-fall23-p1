/**
 * Copyright (C) 2023 Carnegie Mellon University
 *
 * This file is part of the Mixnet course project developed for
 * the Computer Networks course (15-441/641) taught at Carnegie
 * Mellon University.
 *
 * No part of the Mixnet project may be copied and/or distributed
 * without the express permission of the 15-441/641 course staff.
 */
#ifndef FRAMEWORK_NETWORKING_H_
#define FRAMEWORK_NETWORKING_H_

#include "error.h"

#include <memory>
#include <netinet/in.h>
#include <stdint.h>

namespace framework {
namespace networking {

// Helper macros
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                  \
    TypeName(const TypeName&) = delete;                     \
    void operator=(const TypeName&) = delete

/**
 * Per-connection state.
 */
struct accepted_state {
    int connection_fd;                          // FD of the new connection
    socklen_t addrlen;                          // Size of the address field
    sockaddr_in address{};                      // Address of the new client

    explicit accepted_state() : connection_fd(-1),
                                addrlen(sizeof(address)) {}
};

/**
 * Arguments to accept().
 */
struct accept_args {
    // Static fields
    const int listen_fd;                        // FD of socket to listen on
    const bool use_timeout;                     // Use the timeout mechanism
    const uint64_t timeout_ms;                  // Timeout for accept (in ms)
    const uint16_t max_clients;                 // Expected number of clients
    // Dynamic fields
    int rc = 0;                                 // Return code (0 on success)
    uint16_t num_accepted = 0;                  // Number of accepted clients
    volatile bool done = false;                 // Accept routine terminated?
    volatile bool started = false;              // Accept routine initialized
    volatile bool keep_running = true;          // ITC synchronization variable
    std::unique_ptr<accepted_state[]> states{}; // The connected clients' states

    DISALLOW_COPY_AND_ASSIGN(accept_args);
    explicit accept_args(const int listen_fd, const bool use_timeout,
                         const uint64_t timeout, const uint16_t max_clients);
};

/**
 * RX/TX modes, representing three different semantics:
 *
 * 1. Blocking: Block until a message is received. Valid for
 *              both blocking and non-blocking (NB) sockets,
 *              during both RX and TX.
 *
 * 2. Timeout:  Try to read a message with a given timeout,
 *              otherwise fails. Valid only for NB sockets,
 *              during both RX and TX.
 *
 * 3. Try-once: Try once to read a full message, otherwise
 *              fail. Only valid for NB sockets during RX.
 */
enum class mode {
    RX_TX_BLOCKING = 0, RX_TX_TIMEOUT, RX_TRY_ONCE,
};

/**
 * Config for RX/TX functions.
 */
class config final {
private:
    mode mode_ = mode::RX_TX_BLOCKING;
    uint64_t timeout_ms_ = ((uint64_t) -1);

public:
    explicit config(const mode mode, const uint64_t timeout) :
                    mode_(mode), timeout_ms_(timeout) {}
    // Accessors
    uint64_t timeout() const { return timeout_ms_; }
    bool block() const { return (mode_ == mode::RX_TX_BLOCKING); }
    bool try_once() const { return (mode_ == mode::RX_TRY_ONCE); }
    bool use_timeout() const { return (mode_ == mode::RX_TX_TIMEOUT); }
};

int socket(const bool reuse_addr, const bool is_blocking);

error_code server_setup(
    int *socket_fd, sockaddr_in *const addr,
    const int listen_queue, const bool reuse_addr);

void server_accept(accept_args& args);

int connect_with_timeout(
    const int socket_fd, const sockaddr_in *const addr,
    const socklen_t addrlen, const uint64_t timeout_ms);

template<typename T>
error_code send_generic(const config config, const int fd,
    const char *buffer, const error_code connection_error,
                        const T min_len, const T max_len);

template<typename T>
error_code recv_generic(const config config, const int fd,
    char *buffer, const error_code connection_error, const
    T min_len, const T max_len);

bool equal_netaddrs(const sockaddr_in addr_a,
                    const sockaddr_in addr_b);

// Cleanup
#undef DISALLOW_COPY_AND_ASSIGN

} // namespace networking
} // namespace framework

#endif // FRAMEWORK_NETWORKING_H_
