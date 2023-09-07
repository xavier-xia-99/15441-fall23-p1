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
#include "fragment.h"

#include "mixnet/connection.h"
#include "mixnet/node.h"
#include "mixnet/packet.h"
#include "external/argparse/argparse.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

namespace framework {

// Typedefs
using namespace std::chrono;
typedef high_resolution_clock clock;

/**
 * Helper macros.
 */
#define DIE_ON_ERROR(x)                                         \
    error_code = x;                                             \
    if (error_code != error_code::NONE) { return error_code; }

fragment::node_context::
node_context(message_queue& mq_pcap,
             message_queue& mq_user) :
             mq_pcap(mq_pcap), mq_user(mq_user) {
    recv_buffer = std::make_unique<
        char[]>(MAX_MIXNET_PACKET_SIZE);
}

fragment::node_context::~node_context() {
    // Close local TX sockets to neighbors
    for (size_t nid = 0; nid < tx_socket_fds.size(); nid++) {
        if (tx_socket_fds[nid] != -1) { close(tx_socket_fds[nid]); }
    }
    // Close listening socket
    if (tx_listen_fd != -1) {
        close(tx_listen_fd);
    }
    // Close local RX sockets to neighbors
    for (size_t nid = 0; nid < rx_socket_fds.size(); nid++) {
        if (rx_socket_fds[nid] != -1) { close(rx_socket_fds[nid]); }
    }
}

error_code fragment::node_context::_send_blocking(
    const int fd, const char *const b) {
    // Flag accidental type changes
    static_assert(sizeof(uint16_t) ==
                  sizeof(mixnet_packet::total_size));

    networking::config c{networking::mode::RX_TX_BLOCKING, 0};
    return networking::send_generic<uint16_t>(
        c, fd, b, error_code::MIXNET_CONNECTION_BROKEN,
        MIN_MIXNET_PACKET_SIZE, MAX_MIXNET_PACKET_SIZE);
}

error_code fragment::node_context::_recv_once(
    const int fd, char *b) {
    // Flag incorrect type changes
    static_assert(sizeof(uint16_t) ==
                  sizeof(mixnet_packet::total_size));

    networking::config c{networking::mode::RX_TRY_ONCE, 0};
    return networking::recv_generic<uint16_t>(
        c, fd, b, error_code::MIXNET_CONNECTION_BROKEN,
        MIN_MIXNET_PACKET_SIZE, MAX_MIXNET_PACKET_SIZE);
}

int fragment::node_context::node_send(
    const uint8_t port, mixnet_packet *const packet) {
    const uint16_t max_port_id = config.num_neighbors;
    if (port > max_port_id) { return -1; } // Invalid port ID

    // Packet size is out-of-bounds
    if ((packet->total_size < MIN_MIXNET_PACKET_SIZE) ||
        (packet->total_size > MAX_MIXNET_PACKET_SIZE)) {
        return -1;
    }
    // Bleach reserved field
    memset(&(packet->_reserved[0]), 0,
           sizeof(packet->_reserved));

    decltype(packet->total_size) payload_size =
        (packet->total_size - sizeof(mixnet_packet));

    // Validate size specifications
    bool validate_size = true;
    switch (packet->type) {
    case PACKET_TYPE_STP: {
        // Check packet size
        validate_size &= (
            payload_size == sizeof(mixnet_packet_stp));
    } break;

    case PACKET_TYPE_FLOOD: {
        // Check packet size
        validate_size &= (payload_size == 0);
    } break;

    case PACKET_TYPE_LSA: {
        // Check packet size
        mixnet_packet_lsa *lsa = reinterpret_cast<
            mixnet_packet_lsa*>(packet->payload());

        validate_size &= (
            payload_size == (
                (sizeof(mixnet_packet_lsa)) +
                (sizeof(mixnet_lsa_link_params) * lsa->neighbor_count))
        );
    } break;

    case PACKET_TYPE_DATA: {} break;
    case PACKET_TYPE_PING: {
        // Check payload size
        mixnet_packet_routing_header *rh = reinterpret_cast<
            mixnet_packet_routing_header*>(packet->payload());

        validate_size &= (
            payload_size == (
                sizeof(mixnet_packet_ping) +
                sizeof(mixnet_packet_routing_header) +
                (sizeof(mixnet_address) * rh->route_length))
        );
    } break;

    // Unknown packet type
    default: { validate_size = false; } break;
    } // switch
    if (!validate_size) { return -1; }

    // This is the application-level data port
    if (port == max_port_id) {
        if ((packet->type != PACKET_TYPE_FLOOD) &&
            (packet->type != PACKET_TYPE_DATA) &&
            (packet->type != PACKET_TYPE_PING)) {
            return -1;
        }
        // If the orchestrator is subscribed to pcap updates
        // from this node, then mirror this packet to the MQ.
        if (is_pcap_subscribed) {
            void **ptr = ((void**)
                message_queue_message_alloc(&mq_pcap));

            // If the MQ failed to allocate memory, it means the
            // pcap thread isn't consuming fast enough, an issue
            // that would never arise during normal operation.
            // Indicate failure and return.
            if (ptr == NULL) {
                ts.exited = true;
                ts.exit_code = (
                    error_code::FRAGMENT_PCAP_MQ_FULL);

                free(packet);
                throw thread_state::exit_exception();
            }
            *ptr = packet; // Enque the packet
            message_queue_write(&mq_pcap, ptr);
        }
        // Else, simply free the packet
        else { free(packet); }
        return 1;
    }
    // Regular port
    else {
        auto error_code = _send_blocking(
            tx_socket_fds[port], reinterpret_cast<char*>(packet));

        // Send failed, capture error and die
        if (error_code != error_code::NONE) {
            ts.exit_code = error_code;
            ts.exited = true;
            free(packet);

            throw thread_state::exit_exception();
        }
        // Successful transmission
        else { free(packet); return 1; }
    }
    return 0;
}

int fragment::node_context::node_recv(
    uint8_t *const port, mixnet_packet **const ptr) {
    // Try to perform RX on the inputs ports round-robin
    const uint16_t max_port_id = config.num_neighbors;
    const uint16_t stop_idx = rx_port_idx;
    int num_recvd = 0;
    do {
        // This is the user-level port
        if (rx_port_idx == max_port_id) {
            // Consume a packet from the MQ
            mixnet_packet **mq_ptr = reinterpret_cast<
                mixnet_packet**>(message_queue_tryread(&mq_user));

            // Valid packet
            if (mq_ptr != NULL) {
                num_recvd++; *ptr = *mq_ptr; *port = rx_port_idx;
                message_queue_message_free(&mq_user, (void*) mq_ptr);
            }
        }
        // This is a regular port
        else {
            port_mutexes[rx_port_idx].lock();
            if (link_states[rx_port_idx]) {
                auto error_code = _recv_once(
                    rx_socket_fds[rx_port_idx], recv_buffer.get());

                // Unlock mutex
                port_mutexes[rx_port_idx].unlock();

                // Received a valid packet
                if (error_code == error_code::NONE) {
                    mixnet_packet *packet = reinterpret_cast<
                            mixnet_packet*>(recv_buffer.get());
                    num_recvd++;
                    *port = rx_port_idx;

                    // Initialize the packet buffer
                    *ptr = static_cast<mixnet_packet*>(
                            malloc(packet->total_size));

                    memcpy(*ptr, recv_buffer.get(), packet->total_size);
                }
                // Encountered an error on the receive path
                else if (error_code != error_code::RECV_ZERO_PENDING) {
                    ts.exit_code = error_code;
                    ts.exited = true;

                    throw thread_state::exit_exception();
                }
                // Fallthrough: No packets pending
                // for this port, try another one.
            }
            // Unlock mutex
            else { port_mutexes[rx_port_idx].unlock(); }
        }
        // Compute the next index (round-robin)
        rx_port_idx = ((rx_port_idx + 1) %
                       (max_port_id + 1));
    }
    while ((num_recvd == 0) &&
           (rx_port_idx != stop_idx));

    return num_recvd;
}

void fragment::init_node_context(
    message::request::topology *const p) {
    node_context_ = std::make_unique<
        node_context>(*mq_pcap_, *mq_user_);

    mixnet_node_config *config = &(
                node_context_->config);

    // Initialize mixnet config
    config->node_addr = p->mixaddr;
    config->num_neighbors = p->num_neighbors;
    config->mixing_factor = p->mixing_factor;
    config->do_random_routing = p->do_random_routing;
    config->root_hello_interval_ms = p->root_hello_interval_ms;
    config->reelection_interval_ms = p->reelection_interval_ms;

    const uint16_t num_neighbors = config->num_neighbors;
    if (num_neighbors == 0) { return; } // Nothing to do

    config->link_costs = new uint16_t[num_neighbors];
    memcpy(config->link_costs, p->link_costs(),
           sizeof(uint16_t) * num_neighbors);

    // Allocate port mutexes
    node_context_->port_mutexes = (
        std::make_unique<std::mutex[]>(num_neighbors));

    // Allocate miscellaneous state
    node_context_->tx_socket_fds.resize(num_neighbors, -1);
    node_context_->rx_socket_fds.resize(num_neighbors, -1);
    node_context_->link_states.resize(num_neighbors, true);
    node_context_->neighbor_netaddrs.resize(num_neighbors, sockaddr_in{});
}

void fragment::destroy_node_context() {
    // Free allocated resources and reset context
    delete [] node_context_->config.link_costs;
    node_context_.reset();
}

/**
 * Helper functions to send/recv data.
 */
void fragment::prepare_header(message& m, const message::type type,
                              const error_code error_code) const {
    m.set_error_code(error_code);
    m.set_code(false, type);
    m.set_fragment_id(fid_);
}

error_code fragment::check_header(
    const bool check_id, const bool check_type,
    const message& m, const message::type type) const {

    // Canary does not match expectation
    if (!m.validate()) {
        return error_code::MALFORMED_MESSAGE;
    }
    // Unexpected direction
    else if (!m.is_request()) {
        return error_code::BAD_MESSAGE_CODE;
    }
    // The message FID doesn't match the fragment ID
    else if (check_id && (m.get_fragment_id() != fid_)) {
        return error_code::BAD_FRAGMENT_ID;
    }
    // Received an unexpected message code
    else if (check_type && (m.get_type() != type)) {
        return error_code::BAD_MESSAGE_CODE;
    }
    // The orchestrator reported a framework error
    else if (m.get_error_code() != error_code::NONE) {
        return error_code::FRAGMENT_EXCEPTION;
    }
    return error_code::NONE;
}

error_code fragment::_send_blocking(const int
    fd, const message& m, const error_code e) {

    using namespace networking;
    config c{mode::RX_TX_BLOCKING, 0};
    return send_generic<message::length_t>(c, fd, m.buffer(), e,
        message::MIN_MESSAGE_LENGTH, message::MAX_MESSAGE_LENGTH);
}

error_code fragment::_recv_blocking(const
    int fd, message& m, const error_code e) {

    using namespace networking;
    config c{mode::RX_TX_BLOCKING, 0};
    return recv_generic<message::length_t>(c, fd, m.buffer(), e,
        message::MIN_MESSAGE_LENGTH, message::MAX_MESSAGE_LENGTH);
}

error_code fragment::send_response(const bool is_ctrl,
    const message::type type, const error_code ec,
    const std::function<error_code(message&)>& lambda) {
    auto error_code = error_code::NONE;

    // Latch to the appropriate plane
    auto connection_error = (is_ctrl ?
        error_code::CTRL_CONNECTION_BROKEN :
        error_code::PCAP_CONNECTION_BROKEN);

    message& msg = is_ctrl ? msg_ctrl_ : msg_pcap_;
    const int fd = is_ctrl ? local_fd_ctrl_ : local_fd_pcap_;

    // Prepare header, populate payload
    prepare_header(msg, type, ec);
    DIE_ON_ERROR(lambda(msg));
    msg.finalize();

    // Send the response
    return _send_blocking(fd, msg, connection_error);
}

error_code fragment::recv_request(const bool is_ctrl,
    const bool check_id, const bool check_type, const
    message::type type, const std::function<error_code(
        const message&)>& lambda) {
    auto error_code = error_code::NONE; // Retval

    // Latch to the appropriate plane
    auto connection_error = (is_ctrl ?
        error_code::CTRL_CONNECTION_BROKEN :
        error_code::PCAP_CONNECTION_BROKEN);

    message& msg = is_ctrl ? msg_ctrl_ : msg_pcap_;
    const int fd = is_ctrl ? local_fd_ctrl_ : local_fd_pcap_;

    // Receive message
    DIE_ON_ERROR(_recv_blocking(fd, msg, connection_error));
    DIE_ON_ERROR(check_header(check_id, check_type, msg, type));

    // Apply lambda
    return lambda(msg);
}

/**
 * FSM functionality.
 */
error_code fragment::run_state_setup_ctrl() {
    using namespace networking;
    assert(state_ == state_t::SETUP_CTRL);
    auto error_code = error_code::NONE; // Retval

    // Set up a socket to communicate on the ctrl overlay
    if ((local_fd_ctrl_ = socket(false, true)) < 0) {
        return error_code::SOCKET_CREATE_FAILED;
    }
    // Attempt to connect to the orchestrator
    if (connect_with_timeout(local_fd_ctrl_,
        &orc_netaddr_, sizeof(orc_netaddr_),
        timeout_connect_long_) < 0) {
        return error_code::SOCKET_CONNECT_FAILED;
    }

    // Connected, initiate handshake
    auto send_lambda = [this] (message& msg) {
        msg.payload<message::response
            ::setup_ctrl>()->pid = getpid();

        return error_code::NONE;
    };
    DIE_ON_ERROR(
        send_response(true, message::type::SETUP_CTRL,
                      error_code::NONE, send_lambda));

    // Complete handshake
    auto recv_lambda = [this] (const message& msg) {
        fid_ = msg.get_fragment_id();
        autotest_mode_ = msg.payload<
            message::request::setup_ctrl>()->autotest_mode;

        return error_code::NONE;
    };
    DIE_ON_ERROR(recv_request(true, false, true,
        message::type::SETUP_CTRL, recv_lambda));

    if (!autotest_mode_) {
        std::cout << "[Fragment] Connected to orchestrator, "
                  << "assigned ID " << fid_ << std::endl;
    }
    return error_code::NONE;
}

error_code fragment::run_state_setup_pcap() {
    using namespace networking;
    assert(state_ == state_t::SETUP_PCAP);
    auto error_code = error_code::NONE; // Retval

    sockaddr_in pcap_netaddr; // Await ctrl message to start setup
    auto recv_lambda = [this, &pcap_netaddr] (const message& m) {
        auto payload = m.payload<message::request::setup_pcap>();
        pcap_netaddr = payload->pcap_netaddr;
        pcap_netaddr.sin_addr.s_addr = orc_netaddr_.sin_addr.s_addr;

        return error_code::NONE;
    };
    DIE_ON_ERROR(recv_request(true, true, true,
        message::type::SETUP_PCAP, recv_lambda));

    // Acknowledge pcap overlay setup
    DIE_ON_ERROR(send_response(true,
        message::type::SETUP_PCAP, error_code::NONE,
        [](message&) { return error_code::NONE; }));

    // Set up a socket to communicate on the pcap overlay
    if ((local_fd_pcap_ = socket(false, true)) < 0) {
        return error_code::SOCKET_CREATE_FAILED;
    }
    // Attempt to connect to the orchestrator on the pcap plane
    if (connect_with_timeout(local_fd_pcap_,
        &pcap_netaddr, sizeof(pcap_netaddr),
        timeout_connect_short_) < 0) {
        return error_code::SOCKET_CONNECT_FAILED;
    }
    // Connected, initiate handshake
    DIE_ON_ERROR(send_response(false,
        message::type::SETUP_PCAP, error_code::NONE,
        [](message&) { return error_code::NONE; }));

    // Complete handshake
    return recv_request(
        false, true, true, message::type::SETUP_PCAP,
        [](const message&) { return error_code::NONE; });
}

error_code fragment::run_state_create_topology() {
    assert(state_ == state_t::CREATE_TOPOLOGY);
    auto error_code = error_code::NONE; // Retval

    // Await and process the topology ctrl message
    auto recv_lambda = [this] (const message& m) {
        init_node_context(m.payload<
            message::request::topology>());

        return error_code::NONE;
    };
    DIE_ON_ERROR(recv_request(true, true, true,
        message::type::TOPOLOGY, recv_lambda));

    // Acknowledge topology setup
    return send_response(true,
        message::type::TOPOLOGY, error_code::NONE,
        [](message&) { return error_code::NONE; });
}

// Helper macros
#define DIE_DURING_ACCEPT(x)                                    \
    error_code = x;                                             \
    if (error_code != error_code::NONE) {                       \
        node_accept_args_->keep_running = false;                \
        if (node_accept_thread_.joinable()) {                   \
            node_accept_thread_.join();                         \
        }                                                       \
        return error_code;                                      \
    }

error_code fragment::run_state_start_mixnet_server() {
    assert(state_ == state_t::START_MIXNET_SERVER);
    auto error_code = error_code::NONE; // Return value

    // Await the ctrl message
    DIE_ON_ERROR(recv_request(
        true, true, true, message::type::START_MIXNET_SERVER,
        [](const message&) { return error_code::NONE; } ));

    // Set up the node's TX server
    node_context_->tx_server_netaddr.sin_family = AF_INET;
    node_context_->tx_server_netaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    DIE_ON_ERROR(
        networking::server_setup(
            &(node_context_->tx_listen_fd),
            &(node_context_->tx_server_netaddr),
            node_context_->config.num_neighbors, false));

    node_accept_args_ = (
        std::make_unique<networking::accept_args>(
            node_context_->tx_listen_fd, false, 0,
            node_context_->config.num_neighbors));

    // Thread to accept connections
    node_accept_thread_ = std::thread(
        networking::server_accept,
        std::ref(*node_accept_args_));

    // Wait until the thread is running
    while (!node_accept_args_->started) {}

    // Acknowledge server startup
    auto send_lambda = [this] (message& msg) {
        auto payload = msg.payload<message::response::start_mixnet_server>();
        payload->server_netaddr = node_context_->tx_server_netaddr;
        payload->server_netaddr.sin_addr.s_addr = ((uint32_t) -1);
        return error_code::NONE;
    };
    DIE_DURING_ACCEPT(send_response(true,
        message::type::START_MIXNET_SERVER,
            error_code::NONE, send_lambda));

    return error_code;
}

error_code fragment::run_state_start_mixnet_client() {
    using namespace networking;
    assert(state_ == state_t::START_MIXNET_CLIENT);
    auto error_code = error_code::NONE; // Return value
    const uint16_t num_neighbors = node_context_->config.num_neighbors;

    // Await the ctrl message
    auto recv_lambda = [this,
        num_neighbors] (const message& m) {
        auto payload = m.payload<message::
            request::start_mixnet_clients>();

        if (num_neighbors != payload->num_neighbors) {
            return error_code::FRAGMENT_BAD_NEIGHBOR_COUNT;
        }
        // Update the neighbors' network addresses
        for (uint16_t nid = 0; nid < num_neighbors; nid++) {
            node_context_->neighbor_netaddrs[nid] = (
                payload->neighbor_server_netaddrs()[nid]);
        }
        return error_code::NONE;
    };
    DIE_DURING_ACCEPT(recv_request(true, true, true,
        message::type::START_MIXNET_CLIENTS, recv_lambda));

    // Next, attempt to connect to each neighbor
    for (uint16_t nid = 0; nid < num_neighbors; nid++) {
        if ((node_context_->rx_socket_fds[nid] = (
                socket(false, false))) < 0) {
            DIE_DURING_ACCEPT(error_code::SOCKET_CREATE_FAILED);
        }
        // Attempt to connect to the neighbor's Mixnet server
        sockaddr_in netaddr = node_context_->neighbor_netaddrs[nid];
        if (connect_with_timeout(node_context_->rx_socket_fds[nid],
            &(netaddr), sizeof(netaddr), timeout_connect_short_) < 0)
            { DIE_DURING_ACCEPT(error_code::SOCKET_CONNECT_FAILED); }
    }
    auto start = clock::now();
    int64_t timer = timeout_connect_short_;
    auto deadline = start + milliseconds(timer);

    do {
        timer = duration_cast<milliseconds>(
            deadline - clock::now()).count();
    }
    // Wait until timeout or accept thread finishes
    while (!node_accept_args_->done && (timer > 0));
    node_accept_args_->keep_running = false;
    node_accept_thread_.join();

    // Ensure that all the neighbors connected successfully
    if (node_accept_args_->num_accepted != num_neighbors) {
        DIE_DURING_ACCEPT(error_code::SOCKET_ACCEPT_TIMEOUT);
    }
    auto send_lambda = [this, num_neighbors] (message& msg) {
        auto payload = msg.payload<message::
            response::start_mixnet_clients>();

        bool success = true;
        payload->num_neighbors = num_neighbors;
        auto client_netaddrs = payload->client_netaddrs();
        for (uint16_t nid = 0; (nid < num_neighbors) && success; nid++) {
            socklen_t addrlen = sizeof(sockaddr);
            success &= (getsockname(node_context_->rx_socket_fds[nid],
                        (sockaddr*) &(client_netaddrs[nid]), &addrlen) == 0);
        }
        return success ? error_code::NONE :
                         error_code::SOCKET_OPTIONS_FAILED;
    };
    DIE_DURING_ACCEPT(
        send_response(true,
            message::type::START_MIXNET_CLIENTS,
                error_code::NONE, send_lambda));

    return error_code;
}

error_code fragment::run_state_resolve_mixnet_connections() {
    auto error_code = error_code::NONE; // Return value
    assert(state_ == state_t::RESOLVE_MIXNET_CONNECTIONS);
    const uint16_t num_neighbors = node_context_->config.num_neighbors;

     // Await the ctrl message
    auto recv_lambda = [this,
        num_neighbors] (const message& m) {
        auto payload = m.payload<message::
            request::resolve_mixnet_connections>();

        if (payload->num_neighbors != num_neighbors) {
            return error_code::FRAGMENT_BAD_NEIGHBOR_COUNT;
        }
        // Map NIDs to the appropriate local server FDs
        for (uint16_t nid = 0; nid < num_neighbors; nid++) {
            bool success = false;
            for (uint16_t i = 0; i < num_neighbors; i++) {
                if (networking::equal_netaddrs(
                    node_accept_args_->states[i].address,
                    payload->neighbor_client_netaddrs()[nid])) {

                    node_context_->tx_socket_fds[nid] = (
                        node_accept_args_->states[i].connection_fd);

                    success = true; break;
                }
            }
            // Sanity check: ensure consistent adjacency relationship
            if (!success) { return error_code::FRAGMENT_EXCEPTION; }
        }
        return error_code::NONE;
    };
    DIE_DURING_ACCEPT(recv_request(true, true, true,
        message::type::RESOLVE_MIXNET_CONNS, recv_lambda));

    // Acknowledge connection resolution
    return send_response(true, message::type::RESOLVE_MIXNET_CONNS,
        error_code::NONE, [](message&) { return error_code::NONE; });
}
// Cleanup
#undef DIE_DURING_ACCEPT

error_code fragment::run_state_start_testcase() {
    assert(state_ == state_t::START_TESTCASE);
    auto error_code = error_code::NONE; // Retval

    // Await the ctrl message
    DIE_ON_ERROR(recv_request(
        true, true, true, message::type::START_TESTCASE,
        [](const message&) { return error_code::NONE; } ));

    // Acknowledge testcase start
    return send_response(true, message::type::START_TESTCASE,
        error_code::NONE, [](message&) { return error_code::NONE; });
}

error_code fragment::run_state_run_testcase() {
    using namespace networking;

    assert(state_ == state_t::RUN_TESTCASE);
    auto error_code = error_code::NONE;
    bool end_testcase = false;

    // Launch the helper threads
    thread_node_ = std::thread(&fragment::worker_node, this);
    thread_pcap_ = std::thread(&fragment::worker_pcap, this);

    while ((error_code == error_code::NONE) && !end_testcase) {
        bool do_respond = false;
        error_code = (recv_request(
            true, true, false, message::type::NOOP,
            [](const message&) { return error_code::NONE; } ));

        // Received a valid message
        if (error_code == error_code::NONE) {
            auto type = msg_ctrl_.get_type();
            switch (type) {

            // Change a network link state
            case message::type::CHANGE_LINK_STATE: {
                auto payload = msg_ctrl_.payload<
                    message::request::change_link_state>();

                error_code = task_update_link_state(
                    payload->neighbor_id, payload->state);

                do_respond = true;
            } break;

            // Change the pcap subscription
            case message::type::PCAP_SUBSCRIPTION: {
                error_code = task_update_pcap_subscription(
                    msg_ctrl_.payload<message::request::
                        pcap_subscription>()->subscribe);

                do_respond = true;
            } break;

            // Perform packet injection
            case message::type::SEND_PACKET: {
                error_code = task_send_packet(msg_ctrl_.
                    payload<message::request::send_packet>());

                do_respond = true;
            } break;

            // End this testcase
            case message::type::END_TESTCASE: {
                end_testcase = true;
                error_code = task_end_testcase();

                // Only respond if we can clean up ourselves
                if (error_code == error_code::NONE) {
                    do_respond = true;
                }
            } break;

            default: {
                // Received an unexpected message
                error_code = error_code::BAD_MESSAGE_CODE;
            } break;
            } // switch

            if (do_respond) {
                // Send a response to the orchestrator
                error_code = send_response(true, type, error_code,
                        [](message&) { return error_code::NONE; });
            }
        }
        // Didn't receive a message
        else if (error_code == error_code::RECV_WAIT_TIMEOUT) {
            assert(false); // Impossible in blocking mode
        }
        // Irrecoverable error
        else { break; }

        // Check on the helper threads. If either of them exited prematurely,
        // log the relevant error, then wait for the orchestrator to timeout.
        if (!end_testcase && (node_context_->ts.exited || ts_pcap_.exited)) {
            if (node_context_->ts.exit_code != error_code::NONE) {
                std::cout << "[Fragment " << fid_ << "] node thread exited with error "
                          << static_cast<uint16_t>(node_context_->ts.exit_code)
                          << std::endl;
            }
            if (ts_pcap_.exit_code != error_code::NONE) {
                std::cout << "[Fragment " << fid_ << "] pcap thread exited with error "
                          << static_cast<uint16_t>(ts_pcap_.exit_code) << std::endl;
            }
            error_code = error_code::FRAGMENT_EXCEPTION;
        }
    }
    return error_code;
}

// Here, check if we can clean up gracefully. In particular,
// if we are sure that the helper threads can be terminated
// by signalling, then we can clean up ourselves. Otherwise
// it's better NOT to reply and wait for the orchestrator's
// timeout mechanism to kick in and clean up for us.
error_code fragment::task_end_testcase() {
    node_context_->ts.keep_running = false;
    ts_pcap_.keep_running = false;

    // Enqueue an empty pointer in the pcap queue to signal completion
    void **ptr = (void**) message_queue_message_alloc(mq_pcap_.get());
    if (ptr == NULL) { return error_code::FRAGMENT_EXCEPTION; }
    *ptr = NULL;
    message_queue_write(mq_pcap_.get(), (void*) ptr);

    // Give threads some time, if required
    if (!node_context_->ts.exited || !ts_pcap_.exited) { sleep(1); }

    // Nope, still running
    if (!node_context_->ts.exited || !ts_pcap_.exited) {
        return error_code::FRAGMENT_THREADS_NONRESPONSIVE;
    }
    thread_pcap_.join();
    thread_node_.join();
    return error_code::NONE;
}

error_code fragment::run_state_do_shutdown() {
    assert(state_ == state_t::SHUTDOWN);
    auto error_code = error_code::NONE; // Retval

    DIE_ON_ERROR(
        recv_request(true, true, true, message::type::SHUTDOWN,
                [](const message&) { return error_code::NONE; }));

    error_code = send_response(true, message::type::SHUTDOWN,
        error_code::NONE, [](message&) { return error_code::NONE; });

    // Destroy the node context and return
    if (error_code == error_code::NONE) {
        destroy_node_context();
    }
    return error_code;
}

void fragment::worker_node() {
    thread_state& ts = node_context_->ts;
    ts.started = true; // Indicate thread starting

    try {
        // Invoke the students' node implementation. Since this is
        // external code, assume that we have no control over what
        // happens (at least in this thread) this point forth. The
        // fragment will opportunistically try to exit gracefully,
        // but we'll otherwise rely on the orchestrator's timeout
        // mechanism to handle any catastrophic failures.
        run_node(node_context_.get(), &(ts.keep_running),
                 node_context_->config);
    }
    catch (const thread_state::exit_exception& e) {}
    ts.exited = true;
}

void fragment::worker_pcap() {
    thread_state& ts = ts_pcap_;
    auto error_code = error_code::NONE;

    while (ts.keep_running && (error_code == error_code::NONE)) {
        // Consume packets from the pcap MQ. Since we want this
        // thread to yield when it is not doing anything useful,
        // we use blocking read operations. Finally, to prevent
        // deadlocks scenarios (possible if the producer thread
        // itself exits), we use the NULL pointer as a sentinel
        // value, signalling (in-band) that the queue is out of
        // operation and the thread should return.
        void *ptr = message_queue_read(mq_pcap_.get());
        mixnet_packet *packet = *(static_cast<
                        mixnet_packet**>(ptr));

        message_queue_message_free(mq_pcap_.get(), ptr);
        if (packet == NULL) { break; } // Got sentinel, done
        assert(packet->total_size <= MAX_MIXNET_PACKET_SIZE);

        auto send_lambda = [this, packet] (message& msg) {
            auto payload = msg.payload<mixnet_packet>();
            memcpy(payload, packet, packet->total_size);
            return error_code::NONE;
        };
        error_code = send_response(false, message::type::PCAP_DATA,
                                   error_code::NONE, send_lambda);
        ts.exit_code = error_code;
        free(packet);
    }
    ts.exited = true;
}

void fragment::run() {
    assert(state_ == state_t::SETUP_CTRL);
    auto next_state = state_t::SETUP_CTRL;
    error_code error_code = error_code::NONE;

    do {
        switch (state_) {
        // Setup ctrl overlay
        case state_t::SETUP_CTRL: {
            error_code = run_state_setup_ctrl();
            next_state = state_t::SETUP_PCAP;
        } break;

        // Setup pcap overlay
        case state_t::SETUP_PCAP: {
            error_code = run_state_setup_pcap();
            next_state = state_t::CREATE_TOPOLOGY;
        } break;

        // Create Mixnet topology
        case state_t::CREATE_TOPOLOGY: {
            error_code = run_state_create_topology();
            next_state = state_t::START_MIXNET_SERVER;
        } break;

        // Start Mixnet server
        case state_t::START_MIXNET_SERVER: {
            error_code = run_state_start_mixnet_server();
            next_state = state_t::START_MIXNET_CLIENT;
        } break;

        // Start Mixnet client
        case state_t::START_MIXNET_CLIENT: {
            error_code = run_state_start_mixnet_client();
            next_state = state_t::RESOLVE_MIXNET_CONNECTIONS;
        } break;

        // Resolve Mixnet connections
        case state_t::RESOLVE_MIXNET_CONNECTIONS: {
            error_code = run_state_resolve_mixnet_connections();
            next_state = state_t::START_TESTCASE;
        } break;

        // Start testcase
        case state_t::START_TESTCASE: {
            error_code = run_state_start_testcase();
            next_state = state_t::RUN_TESTCASE;
        } break;

        // Run the testcase
        case state_t::RUN_TESTCASE: {
            error_code = run_state_run_testcase();
            next_state = state_t::SHUTDOWN;
        } break;

        // Perform graceful shutdown
        case state_t::SHUTDOWN: {
            error_code = run_state_do_shutdown();
            next_state = state_t::DONE;
        } break;

        case state_t::DONE: { break; }

        // Unknown state
        default: { assert(false); } break;
        } // switch

        if ((error_code != error_code::NONE) && !autotest_mode_) {
            std::cout << "[Fragment";
            if (fid_ != INVALID_FRAGMENT_ID) {
                std::cout << " " << fid_;
            }
            std::cout << "] Dying with error code "
                      << static_cast<uint16_t>(error_code)
                      << " at state "
                      << static_cast<uint16_t>(state_)
                      << std::endl;
        }
        // Move to new state
        state_ = next_state;

    } while ((state_ != state_t::DONE) &&
             (error_code == error_code::NONE));

    if (error_code == error_code::NONE) {
        if (!autotest_mode_) {
            std::cout << "[Fragment " << fid_
                      << "] Exiting normally" << std::endl;
        }
    }
    // Can't clean up properly, wait to be killed
    else if (autotest_mode_) { while(true) {} }
}

/**
 * Testcase tasks.
 */
error_code fragment::task_update_pcap_subscription(const bool v) {
    node_context_->is_pcap_subscribed = v; return error_code::NONE;
}

error_code fragment::task_send_packet(
    message::request::send_packet *const p) {
    mixnet_packet *packet = static_cast<mixnet_packet*>(
                        malloc(MAX_MIXNET_PACKET_SIZE));
    if (packet == NULL) {
        return error_code::FRAGMENT_EXCEPTION;
    }
    uint16_t total_size = sizeof(mixnet_packet);
    packet->type = p->type; // Update type

    // If required, initialize other data
    if ((packet->type == PACKET_TYPE_DATA) ||
        (packet->type == PACKET_TYPE_PING)) {

        auto routing_header = reinterpret_cast<
            mixnet_packet_routing_header*>(packet->payload());

        routing_header->src_address = p->src_mixaddr;
        routing_header->dst_address = p->dst_mixaddr;
        routing_header->route_length = 0;
        routing_header->hop_index = 0;

        // Update total packet size to include routing header
        total_size += sizeof(mixnet_packet_routing_header);

        if (packet->type == PACKET_TYPE_DATA) {
            total_size += p->data_length;
            void *data = routing_header->route();
            memcpy(data, p->data(), p->data_length);
        }
    }
    // Set the packet size
    packet->total_size = total_size;

    void **ptr = (void**) message_queue_message_alloc(mq_user_.get());
    // Error out if the node isn't consuming packets fast enough
    if (ptr == NULL) { free(packet);
                       return error_code::FRAGMENT_EXCEPTION; }
    *ptr = packet;
    message_queue_write(mq_user_.get(), (void*) ptr);

    return error_code::NONE;
}

error_code fragment::task_update_link_state(
    const uint16_t nid, const bool state) {
    auto error_code = error_code::NONE; // Retval
    // Sanity check: Orchestrator must sanitize input
    assert(nid < node_context_->config.num_neighbors);

    // Acquire the mutex for the corresponding port and
    // update link state. If the link is being disabled,
    // we also need to drain the socket receive queue.
    node_context_->port_mutexes[nid].lock();
    node_context_->link_states[nid] = state;

    if (!state) {
        do {
            // Drain the receive queue
            error_code = node_context_->_recv_once(
                node_context_->rx_socket_fds[nid],
                msg_ctrl_.buffer());
        }
        while (error_code == error_code::NONE);
    }
    node_context_->port_mutexes[nid].unlock();
    return (error_code == error_code::RECV_ZERO_PENDING) ?
            error_code::NONE : error_code;
}

fragment::fragment(const sockaddr_in& orc_netaddr) :
                   orc_netaddr_(orc_netaddr) {
    // Initialize MQs
    mq_pcap_ = std::make_unique<message_queue>();
    mq_user_ = std::make_unique<message_queue>();
    message_queue_init(mq_pcap_.get(), sizeof(void*), MQ_PCAP_DEPTH);
    message_queue_init(mq_user_.get(), sizeof(void*), MQ_USER_DEPTH);
}

fragment::~fragment() {
    // Destroy MQs
    message_queue_destroy(mq_pcap_.get());
    message_queue_destroy(mq_user_.get());

    // Close local pcap, ctrl sockets
    if (local_fd_pcap_ != -1) {
        close(local_fd_pcap_);
    }
    if (local_fd_ctrl_ != -1) {
        close(local_fd_ctrl_);
    }
}

// Cleanup
#undef DIE_ON_ERROR

} // namespace framework

/**
 * C/C++ bridge functions.
 */
int mixnet_recv(void *h, uint8_t *v, mixnet_packet **p) {
    return static_cast<framework::fragment::
        node_context*>(h)->node_recv(v, p);
}

int mixnet_send(void *h, const uint8_t v, mixnet_packet *p) {
    return static_cast<framework::fragment::
        node_context*>(h)->node_send(v, p);
}

int main(int argc, char **argv) {
    argparse::ArgumentParser program("Node");
    program.add_argument("orchestrator_ip")
                        .help("Orchestrator's public IP address");

    program.add_argument("orchestrator_port")
                        .scan<'u', unsigned int>()
                        .help("Orchestrator's port number");
    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
    // Parse arguments
    in_addr_t s_addr = inet_addr(
        program.get("orchestrator_ip").c_str());

    uint16_t port_number = static_cast<uint16_t>(
        program.get<unsigned int>("orchestrator_port"));

    // Validate arguments
    if (s_addr == (in_addr_t) -1) {
        std::cout << "[Fragment] Invalid server address"
                  << std::endl;
        return 1;
    }
    // Orchestrator address
    sockaddr_in orc_netaddr;
    memset(&orc_netaddr, 0, sizeof(orc_netaddr));
    orc_netaddr.sin_family = AF_INET;

    orc_netaddr.sin_addr.s_addr = s_addr;
    orc_netaddr.sin_port = htons(port_number);

    // Run the main fragment loop
    framework::fragment(orc_netaddr).run();
    return 0;
}
