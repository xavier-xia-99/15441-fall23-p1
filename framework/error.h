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
#ifndef FRAMEWORK_ERROR_H_
#define FRAMEWORK_ERROR_H_

namespace framework {

// Framework error codes
enum class error_code {
    // OK (DO NOT TOUCH)
    NONE = 0,
    // Possible errors
    POLL_ERROR,
    FORK_FAILED,
    EXEC_FAILED,
    BAD_TESTCASE,
    BAD_FRAGMENT_ID,
    BAD_MESSAGE_CODE,
    MALFORMED_MESSAGE,
    RECV_ZERO_PENDING,
    RECV_WAIT_TIMEOUT,
    SEND_REQS_TIMEOUT,
    FRAGMENT_EXCEPTION,
    SOCKET_BIND_FAILED,
    SOCKET_ACCEPT_FAILED,
    SOCKET_CREATE_FAILED,
    SOCKET_LISTEN_FAILED,
    SOCKET_BAD_OPERATION,
    SOCKET_CONNECT_FAILED,
    SOCKET_OPTIONS_FAILED,
    SOCKET_ACCEPT_TIMEOUT,
    FRAGMENT_PCAP_MQ_FULL,
    CTRL_CONNECTION_BROKEN,
    PCAP_CONNECTION_BROKEN,
    FRAGMENT_INVALID_SADDR,
    FRAGMENT_INVALID_CADDR,
    MIXNET_BAD_PACKET_SIZE,
    MIXNET_CONNECTION_BROKEN,
    FRAGMENT_BAD_NEIGHBOR_COUNT,
    FRAGMENT_THREADS_NONRESPONSIVE,
    FRAGMENT_DUPLICATE_LOCAL_ADDRS,
    // Sentinel value (DO NOT TOUCH)
    ENUMERATION_LIMIT,
};

} // namespace framework

#endif // FRAMEWORK_ERROR_H_
