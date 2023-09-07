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
#include "networking.h"

#include "message.h"

#include <assert.h>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace framework {
namespace networking {

// Typedefs
using namespace std::chrono;
typedef high_resolution_clock clock;

accept_args::accept_args(
    const int fd, const bool use_timeout,
    const uint64_t timeout, const uint16_t max_clients) :
    listen_fd(fd), use_timeout(use_timeout), timeout_ms(
        timeout), max_clients(max_clients) { states = std::
                make_unique<accepted_state[]>(max_clients); }

/**
 * Initializes a TCP socket.
 */
int socket(const bool reuse_addr, bool is_blocking) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == -1) { return -1; }
    bool success = true;

    if (!is_blocking) {
        // Configure the socket to be non-blocking
        const int flags = fcntl(socket_fd, F_GETFL, 0);
        success &= (flags >= 0); // Fetch the currently-set flags
        success &= (fcntl(socket_fd, F_SETFL, (flags | O_NONBLOCK)) >= 0);
    }
    // Reuse address if required
    if (reuse_addr) {
        const int reuse = 1;
        success &= (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                               &reuse, sizeof(int)) != -1);
    }
    if (!success) { close(socket_fd); }
    return success ? socket_fd : -1;
}

/**
 * Performs standard server setup (socket, bind, listen).
 */
error_code server_setup(int *socket_fd, sockaddr_in *const addr,
                        const int queue, const bool reuse_addr) {
    bool success = true;
    // No clients, nothing to do
    if (queue == 0) { return error_code::NONE; }

    // Set up a socket to listen for connections
    success &= (*socket_fd = socket(reuse_addr, false)) > 0;
    if (!success) { return error_code::SOCKET_CREATE_FAILED; }

    // Bind to port
    success &= (bind(*socket_fd, (sockaddr *)
                     addr, sizeof(sockaddr)) != -1);
    if (!success) { return error_code::SOCKET_BIND_FAILED; }

    // If binding to arbitrary port, also update addr
    if (addr->sin_port == 0) {
        socklen_t addrlen = sizeof(sockaddr);
        success &= (getsockname((*socket_fd),
                    (sockaddr *) addr, &addrlen) != -1);
    }
    // Prepare to listen for connection attempts
    success &= (listen(*socket_fd, queue) != -1);
    if (!success) { return error_code::SOCKET_LISTEN_FAILED; }

    return error_code::NONE;
}

/**
 * Helper function. Returns ms since the start time-point.
 */
uint64_t get_time_ms_since(const clock::time_point& start) {
    auto delta = clock::now() - start;
    auto ms = duration_cast<milliseconds>(delta);
    return static_cast<uint64_t>(ms.count()); // time (ms)
}

/**
 * Low-level, threadable server-side accept functionality. Attempts to accept
 * a certain number of client connections (max_clients) while a condition var
 * (keep_running) remains true, or timeout is not exhausted. Returns (by ref)
 * the accepted number of clients, local socket fds, and their net addresses.
 * Returns -1 on error (check errno).
 */
void server_accept(accept_args& args) {
    // Sanity check: Ensure args are properly initialized
    assert((args.rc == 0) && (args.num_accepted == 0));
    assert(!args.started && args.keep_running && !args.done);

    args.started = true;
    auto start = clock::now(); // Start accept loop
    while ((args.num_accepted < args.max_clients) &&
           (args.rc == 0) && args.keep_running) {

        if (args.use_timeout) {
            // Exhausted the timeout for connect
            auto delta_ms = get_time_ms_since(start);
            if (delta_ms >= args.timeout_ms) { break; }
        }
        // Accept incoming requests
        accepted_state *const state = &(
            args.states[args.num_accepted]);

        state->connection_fd = -1;
        state->addrlen = sizeof(sockaddr_in);
        int retval = accept(args.listen_fd,
                            (sockaddr *) &(state->address),
                            &(state->addrlen));
        if (retval < 0) {
            // Accept encountered a real error
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                args.rc = -1;
            }
        }
        else {
            // Update the conn_fds array and repeat
            state->connection_fd = retval;
            args.num_accepted += 1;

            // Configure the socket to be non-blocking
            const int flags = fcntl(retval, F_GETFL, 0);
            bool success = (flags >= 0 && fcntl(retval,
                            F_SETFL, (flags | O_NONBLOCK)) >= 0);

            if (!success) { args.rc = -1; }
        }
    }
    args.done = true;
}

/**
 * Client-side connect functionality with timeout and retry. Adapted from:
 * https://stackoverflow.com/a/61960339. Return 0 on success, -1 on error.
 */
int connect_with_timeout(
    const int socket_fd, const sockaddr_in *const address,
    const socklen_t addrlen, const uint64_t timeout_ms) {
    int rc = -1; // Return value

    // First, put the socket into non-blocking mode
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) { return -1; } // Failed to query flags
    if (fcntl(socket_fd, F_SETFL, (flags | O_NONBLOCK)) < 0) { return -1; }

    if ((rc = connect(socket_fd, (sockaddr *) address, addrlen)) < 0) {
        // If connect encountered a real error, this try failed
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK) &&
            (errno != EINPROGRESS)) {
            rc = -1;
        }
        // Connection attempt is still in progress
        else {
            auto start = clock::now();
            do {
                // Exhausted the timeout for connect
                auto delta_ms = get_time_ms_since(start);
                if (delta_ms >= timeout_ms) { rc = 0; break; }

                pollfd pfd; // Set up polling on this fd
                pfd.fd = socket_fd; pfd.events = POLLOUT;
                rc = poll(&pfd, 1, (timeout_ms - delta_ms));

                // If poll 'succeeded', make sure it *really* succeeded
                if(rc > 0) {
                    int error = 0; socklen_t len = sizeof(error);
                    int retval = getsockopt(socket_fd, SOL_SOCKET,
                                            SO_ERROR, &error, &len);

                    if (retval == 0) { errno = error; }
                    if (error != 0) { rc = -1; }
                }
            }
            // Poll was interrupted, retry
            while((rc == -1) && (errno == EINTR));

            // Did poll timeout? If so, this try failed
            if(rc == 0) { errno = ETIMEDOUT; rc = -1; }
        }
    }
    // Finally, restore the original flags and return error code
    if (fcntl(socket_fd, F_SETFL, flags) < 0) { return -1; }
    return rc;
}

/**
 * Given a socket fd, validate config.
 */
static error_code validate(const int fd,
    const config c, const bool is_tx) {

    // Fetch flags set for this socket
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) { return error_code::SOCKET_OPTIONS_FAILED; }

    // Ensure the socket allows the requested semantics
    if (((flags & O_NONBLOCK) == 0) && !c.block()) {
        return error_code::SOCKET_BAD_OPERATION;
    }
    // Ensure op/mode are compatible
    else if (is_tx && c.try_once()) {
        return error_code::SOCKET_BAD_OPERATION;
    }
    return error_code::NONE;
}

/**
 * Generic helper function to receive messages on a socket.
 *
 * Important: Since TCP doesn't guarantee atomic send/receive of
 * application-level messages, we use the following mechanism to
 * achieve a message abstraction: the first sizeof(T) (template
 * param) bytes of any message sent or received via the library
 * functions MUST encode the length (in bytes) of the message.
 *
 * @tparam T: Type (e.g., uint8_t) of the leading length field.
 */
template<typename T>
error_code send_generic(const config config, const int fd,
    const char *buffer, const error_code connection_error,
                        const T min_len, const T max_len) {
    // Sanity checks
    assert(max_len >= min_len);
    assert(min_len >= sizeof(T));

    // Validate config
    auto error_code = validate(fd, config, true);
    const bool use_timeout = config.use_timeout();
    if (error_code != error_code::NONE) { return error_code; }

    // Fetch and validate the message length
    size_t buffer_len = static_cast<size_t>(
            (reinterpret_cast<const T*>(buffer))[0]);

    if ((buffer_len < min_len) || (buffer_len > max_len)) {
        return error_code::MALFORMED_MESSAGE;
    }
    uint64_t delta_ms = 0;
    auto start = clock::now();
    while ((buffer_len != 0) && (!use_timeout ||
                (delta_ms < config.timeout()))) {

        // Attempt to send the message on this socket
        int rc = send(fd, buffer, buffer_len, 0);
        if (rc < 0) {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK) &&
                (errno != ENOBUFS)) { return connection_error; }
        }
        else {
            // Advance the buffer by the TX count
            auto sent_bytes = static_cast<size_t>(rc);
            buffer_len -= sent_bytes; buffer += sent_bytes;
        }
        if (use_timeout) {
            delta_ms = get_time_ms_since(start);
        }
    }
    return ((buffer_len == 0) ?
            error_code::NONE :
            error_code::SEND_REQS_TIMEOUT);
}

/**
 * Generic helper function to send messages on a socket.
 *
 * @tparam T: Type (e.g., uint8_t) of the leading length field.
 */
template<typename T>
error_code recv_generic(const config config, const int fd,
    char *buffer, const error_code connection_error, const
    T min_len, const T max_len) {

    // Sanity checks
    assert(max_len >= min_len);
    assert(min_len >= sizeof(T));

    // Validate config
    auto error_code = validate(fd, config, false);
    const bool use_timeout = config.use_timeout();
    if (error_code != error_code::NONE) { return error_code; }

    int flags = MSG_PEEK;
    bool recv_started = false;
    size_t expected_size = sizeof(T);
    size_t msg_length = 0; // Decoded length

    uint64_t delta_ms = 0;
    auto start = clock::now();
    while (!use_timeout || (delta_ms < config.timeout())) {
        // Attempt to receive a message on this socket
        int rc = recv(fd, buffer, expected_size, flags);
        auto recv_bytes = static_cast<size_t>(rc);

        if (rc < 0) {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK) &&
                (errno != ENOBUFS)) { return connection_error; }

            else if (config.try_once()) {
                // Operation stalls, so give up
                return error_code::RECV_ZERO_PENDING;
            }
        }
        // Socket was closed
        else if (rc == 0) {
            return connection_error;
        }
        // Decoding message length
        else if (msg_length == 0) {
            if (recv_bytes == expected_size) {
                msg_length = static_cast<size_t>(
                    (reinterpret_cast<T*>(buffer))[0]);

                // Packet is malformed (has bad size), exit immediately
                if ((msg_length < min_len) || (msg_length > max_len)) {
                    return error_code::MALFORMED_MESSAGE;
                }
                // Update expected size
                expected_size = msg_length;

                // Blocking, so clear PEEK flag
                if (config.block()) { flags = 0; }
            }
            // Failed once, so give up
            else if (config.try_once()) {
                return error_code::RECV_ZERO_PENDING;
            }
        }
        // In try-once and timeout configurations, we use
        // PEEK to first check whether the receive buffer
        // has sufficient data to return the full message
        // atomically. If so, we then clear the PEEK flag
        // and start consuming the message bytes.
        else if (config.try_once() ||
                 (use_timeout && !recv_started)) {

            // The entire message can be recv'd
            if (recv_bytes == expected_size) {

                // Starting recv, clear PEEK flag
                if (!recv_started) { flags = 0; }
                // Finished recv in try-once mode
                else { return error_code::NONE; }

                recv_started = true;
            }
            else if (config.try_once()) {
                // If recv() was invoked in non-PEEK mode,
                // reading fewer bytes than expected is an
                // irrecoverable error. Else we can simply
                // indicate zero messages pending.
                return (recv_started ? connection_error :
                            error_code::RECV_ZERO_PENDING);
            }
        }
        else {
            // Once we start consuming data from the recv buffer,
            // we are committed to atomically reading the entire
            // message. A timeout this point forth is treated as
            // a WAIT_TIMEOUT error (before this point, it would
            // simply indicate zero pending messages).
            recv_started = true;

            // Advance the receive buffer
            expected_size -= recv_bytes; buffer += recv_bytes;
            if (expected_size == 0) { return error_code::NONE; }
        }
        if (use_timeout) {
            delta_ms = get_time_ms_since(start);
        }
    }
    return (recv_started ? error_code::RECV_WAIT_TIMEOUT :
                           error_code::RECV_ZERO_PENDING);
}

/**
 * Explicit template instantiation.
 */
template
error_code send_generic<uint16_t>(
    const config, const int, const char *,
    const error_code, const uint16_t, const uint16_t);

template
error_code recv_generic<uint16_t>(
    const config, const int, char *,
    const error_code, const uint16_t, const uint16_t);

/**
 * Returns whether two network addresses are identical.
 */
bool equal_netaddrs(const sockaddr_in addr_a,
                    const sockaddr_in addr_b) {
    return ((addr_a.sin_port == addr_b.sin_port) &&
            (addr_a.sin_family == addr_b.sin_family) &&
            (addr_a.sin_addr.s_addr == addr_b.sin_addr.s_addr));
}

} // namespace networking
} // namespace framework
