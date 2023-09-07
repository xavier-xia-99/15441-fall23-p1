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
#include "orchestrator.h"

#include "message.h"
#include "networking.h"
#include "testing/common/testcase.h"

#include <assert.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <poll.h>
#include <signal.h>
#include <string.h>
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

/**
 * Miscellaneous helper methods.
 */
void orchestrator::destroy_sockets() {
    for (size_t fid = 0; fid < fragments_.size(); fid++) {
        if (fragments_[fid].fd_ctrl != -1) {
            close(fragments_[fid].fd_ctrl);
        }
        if (fragments_[fid].fd_pcap != -1) {
            close(fragments_[fid].fd_pcap);
        }
    }
    if (listen_fd_pcap_ != -1) { close(listen_fd_pcap_); }
    if (listen_fd_ctrl_ != -1) { close(listen_fd_ctrl_); }
}

void orchestrator::destroy_fragments(int signal) {
    for (size_t fid = 0; fid < fragments_.size(); fid++) {
        if (fragments_[fid].pid != -1) {
            kill(fragments_[fid].pid, signal);
        }
    }
}

void orchestrator::prepare_header(message& m,
    const uint16_t fid, const message::type type) {
    m.set_fragment_id(fid); m.set_code(true, type);
    m.set_error_code(error_code::NONE);
}

error_code orchestrator::check_header(
    const message& m, const bool check_id,
    const uint16_t fid, const message::type type) {

    // Canary does not match expectation
    if (!m.validate()) {
        return error_code::MALFORMED_MESSAGE;
    }
    // The message FID doesn't match the expected ID
    else if (check_id && (m.get_fragment_id() != fid)) {
        return error_code::BAD_FRAGMENT_ID;
    }
    // Received an unexpected message code
    else if (m.is_request() || (m.get_type() != type)) {
        return error_code::BAD_MESSAGE_CODE;
    }
    // The fragment reported a framework error
    else if (m.get_error_code() != error_code::NONE) {
        return error_code::FRAGMENT_EXCEPTION;
    }
    return error_code::NONE;
}

error_code orchestrator::send_with_timeout(const int fd,
    const message& m, const uint64_t t, const error_code e) {
    using namespace networking;

    config c{mode::RX_TX_TIMEOUT, t};
    return send_generic<message::length_t>(c, fd, m.buffer(), e,
        message::MIN_MESSAGE_LENGTH, message::MAX_MESSAGE_LENGTH);
}

error_code orchestrator::recv_with_timeout(const int fd,
    message& m, const uint64_t t, const error_code e) {
    using namespace networking;

    config c{mode::RX_TX_TIMEOUT, t};
    return recv_generic<message::length_t>(c, fd, m.buffer(), e,
        message::MIN_MESSAGE_LENGTH, message::MAX_MESSAGE_LENGTH);
}

error_code orchestrator::fragment_request_response(
    const uint16_t fid, const message::type type,
    const std::function<void(message&)>& lambda) {
    auto error_code = error_code::NONE;

    // Prepare the header, populate the payload
    prepare_header(msg_ctrl_, fid, type);
    lambda(msg_ctrl_); // Apply lambda
    msg_ctrl_.finalize();

    // Send the request
    int fd_ctrl = fragments_[fid].fd_ctrl;
    DIE_ON_ERROR(send_with_timeout(fd_ctrl,
        msg_ctrl_, timeout_communication_ms_,
        error_code::CTRL_CONNECTION_BROKEN));

    // Wait for response
    DIE_ON_ERROR(recv_with_timeout(fd_ctrl,
        msg_ctrl_, timeout_communication_ms_,
        error_code::CTRL_CONNECTION_BROKEN));

    return check_header(msg_ctrl_, true, fid, type);
}

error_code orchestrator::foreach_fragment_send_ctrl(
    const message::type type, const std::function<void
                (const uint16_t, message&)>& lambda) {

    std::vector<int> fds; // Prepare fds to use with helper
    for (const auto& v : fragments_) { fds.push_back(v.fd_ctrl); }
    return foreach_fragment_send_generic(fds, type, lambda,
                                         error_code::CTRL_CONNECTION_BROKEN);
}

error_code orchestrator::foreach_fragment_send_generic(
    const std::vector<int>& fds, const message::type type,
    const std::function<void(const uint16_t, message&)>&
        lambda, const error_code connection_error) {
    auto error_code = error_code::NONE; // Return value
    prepare_header(msg_ctrl_, 0, type); // Common header
    uint16_t num_pending = fds.size();

    auto start = clock::now();
    int64_t timer = timeout_communication_ms_;
    auto deadline = start + milliseconds(timer);

    // Attempt to send message to every fragment
    for (size_t idx = 0; (idx < fds.size()) &&
                         (timer > 0); idx++) {

        // Update fragment ID and payload
        msg_ctrl_.set_fragment_id(idx);
        lambda(idx, msg_ctrl_);
        msg_ctrl_.finalize();

        auto ec = send_with_timeout(fds[idx],
            msg_ctrl_, timer, connection_error);

        // Accumulate errors across iterations
        if (error_code == error_code::NONE) {
            error_code = ec;
        }
        num_pending--;
        timer = duration_cast<milliseconds>(
            deadline - clock::now()).count();
    }
    if ((error_code == error_code::NONE) && (num_pending != 0)) {
        return error_code::SEND_REQS_TIMEOUT;
    }
    return error_code;
}

error_code orchestrator::foreach_fragment_recv_ctrl(
    const message::type type, const std::function<
        error_code(const uint16_t, const message&)>& lambda) {

    std::vector<int> fds; // Prepare fds to use with helper
    for (const auto& v : fragments_) { fds.push_back(v.fd_ctrl); }
    return foreach_fragment_recv_generic(true, fds, type, lambda,
                                         error_code::CTRL_CONNECTION_BROKEN);
}

error_code orchestrator::foreach_fragment_recv_generic(
    const bool check_fragment_ids, const std::vector<int>& fds,
    const message::type type, const std::function<error_code(
        const uint16_t, const message&)>& lambda,
        const error_code connection_error) {

    auto error_code = error_code::NONE;
    const size_t num_fds = fds.size();

    // Pending responses
    size_t num_pending = num_fds;
    std::unique_ptr<pollfd[]> pfds = (
        std::make_unique<pollfd[]>(num_fds));

    // Initialize pollfd array
    for (size_t idx = 0; idx < num_fds; idx++) {
        pfds[idx].events = POLLIN;
        pfds[idx].fd = fds[idx];
        pfds[idx].revents = 0;
    }

    auto start = clock::now();
    int64_t timer = timeout_communication_ms_;
    auto deadline = start + milliseconds(timer);

    while ((num_pending != 0) && (timer > 0)) {
        int rc = poll(pfds.get(), num_fds, timer);
        if (rc == 0) { break; } // Timeout
        else if (rc > 0) {
            assert(static_cast<size_t>(rc) <= num_pending);
            for (size_t idx = 0; idx < num_fds; idx++) {
                if ((pfds[idx].revents == POLLIN) &&
                    (pfds[idx].fd > 0)) {

                    auto current_ec = recv_with_timeout(
                        fds[idx], msg_ctrl_, timer, connection_error);

                    // Recv'd message correctly, check header
                    if (current_ec == error_code::NONE) {
                        current_ec = check_header(msg_ctrl_,
                            check_fragment_ids, idx, type);
                    }
                    // Header check succeeded, process payload
                    if (current_ec == error_code::NONE) {
                        current_ec = lambda(idx, msg_ctrl_);
                    }
                    // Accumulate errors across iterations
                    if (error_code == error_code::NONE) {
                        error_code = current_ec;
                    }
                    // Disable polling for this FD
                    pfds[idx].events = 0;
                    pfds[idx].fd = -1;
                    num_pending--;
                }
            }
        }
        timer = duration_cast<milliseconds>(
            deadline - clock::now()).count();
    }
    if ((error_code == error_code::NONE) && (num_pending != 0)) {
        return error_code::RECV_WAIT_TIMEOUT;
    }
    return error_code;
}

/**
 * FSM functionality: SEND methods.
 */
error_code orchestrator::run_state_init() {
    assert(state_ == state_t::INIT);
    assert(fragments_.empty()); // Sanity checks

    if (!autotest_mode_) { // Info
        std::cout << "[Orchestrator] Started listening on port "
                  << PORT_LISTEN_CTRL << std::endl;
    }
    return error_code::NONE;
}

error_code orchestrator::run_state_setup_ctrl() {
    using namespace networking;
    assert(state_ == state_t::SETUP_CTRL_PLANE);
    assert(fragments_.empty()); // Sanity checks
    auto error_code = error_code::NONE; // Return value
    const uint16_t num_nodes = testcase_->get_graph().num_nodes;

    // Ctrl server address
    sockaddr_in ctrl_netaddr{};
    ctrl_netaddr.sin_family = AF_INET;
    ctrl_netaddr.sin_port = htons(PORT_LISTEN_CTRL);
    ctrl_netaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Setup the ctrl server
    DIE_ON_ERROR(server_setup(&listen_fd_ctrl_,
                 &ctrl_netaddr, num_nodes, true));

    // Spawn thread to accept new connections
    accept_args args(listen_fd_ctrl_, true,
            timeout_connect_ms_, num_nodes);

    std::thread accept_thread(server_accept,
                              std::ref(args));
    while (!args.started) {}

    bool success = true;
    if (autotest_mode_) {
        // In autotest mode, fork and exec the fragment processes
        for (size_t idx = 0; (idx < num_nodes) && success; idx++) {
            pid_t pid = fork(); // Clone the current process
            if (pid < 0) {
                success = false;
            }
            else if (pid == 0) {
                // Child process
                auto listen_port = std::to_string(PORT_LISTEN_CTRL);
                auto node_path = fragment_dir_ + "/node";
                char *const argv_list[] = {
                    const_cast<char*>(node_path.c_str()),   // 0: Executable path
                    const_cast<char*>("127.0.0.1"),         // 1: Loopback IP
                    const_cast<char*>(listen_port.c_str()), // 2: Server port
                    NULL
                };

                execv(node_path.c_str(), argv_list);
                exit(EXIT_FAILURE);
            }
            else {
                // Parent process
                fragments_.push_back(fragment_metadata());
                fragments_[idx].pid = pid;
            }
        }
    }
    else { fragments_.resize(num_nodes); }
    accept_thread.join();

    // Exit on error
    if (!success) {
        return error_code::FORK_FAILED;
    }
    else if (args.rc != 0) {
        return error_code::SOCKET_ACCEPT_FAILED;
    }
    else if (args.num_accepted != num_nodes) {
        return error_code::SOCKET_ACCEPT_TIMEOUT;
    }
    // Sanity check
    assert(fragments_.size() == num_nodes);

    // Next, wait for fragments to perform handshakes on the
    // ctrl overlay. We will use fragments' messages to map
    // local ctrl fds (returned by accept) to fragment IDs.
    std::vector<int> fds;
    for (size_t idx = 0; idx < fragments_.size(); idx++) {
        fds.push_back(args.states[idx].connection_fd);
    }

    auto recv_lambda = [this, fds] (
        const uint16_t idx, const message& m) {
        auto payload = m.payload<message::response::setup_ctrl>();

        // In autotest mode, assign the fragment ID based on the
        // PID of the fragment process (in the response payload).
        if (autotest_mode_) {
            for (size_t fid = 0; fid < fragments_.size(); fid++) {
                if (fragments_[fid].pid == payload->pid) {

                    // Fragment PIDs should be unique
                    if (fragments_[fid].fd_ctrl != -1) {
                        return error_code::FRAGMENT_EXCEPTION;
                    }
                    fragments_[fid].fd_ctrl = fds[idx];
                    return error_code::NONE;
                }
            }
            // No matching PID found
            return error_code::FRAGMENT_EXCEPTION;
        }
        // In non-autotester mode, assign fragment IDs based
        // on the order of connection with the orchestrator.
        else {
            // Lambda should be invoked once for each idx
            if (fragments_[idx].fd_ctrl != -1) {
                return error_code::FRAGMENT_EXCEPTION;
            }
            fragments_[idx].fd_ctrl = fds[idx];
            return error_code::NONE;
        }
    };
    // Test connectivity on the ctrl overlay
    DIE_ON_ERROR(foreach_fragment_recv_generic(
        false, fds, message::type::SETUP_CTRL,
        recv_lambda, error_code::CTRL_CONNECTION_BROKEN));

    auto send_lambda = [this] (const uint16_t, message& m) {
        auto payload = m.payload<message::request::setup_ctrl>();
        payload->autotest_mode = autotest_mode_;
    };
    // Complete handshake
    return foreach_fragment_send_ctrl(
        message::type::SETUP_CTRL, send_lambda);
}

// Helper macros
#define DIE_DURING_ACCEPT(x)                                    \
    error_code = x;                                             \
    if (error_code != error_code::NONE) {                       \
        args.keep_running = false;                              \
        accept_thread.join();                                   \
        return error_code;                                      \
    }

error_code orchestrator::run_state_setup_pcap() {
    using namespace networking;
    assert(state_ == state_t::SETUP_PCAP_PLANE);
    auto error_code = error_code::NONE; // Return value
    const uint16_t num_nodes = testcase_->get_graph().num_nodes;

    // Pcap server address
    sockaddr_in pcap_netaddr{};
    pcap_netaddr.sin_family = AF_INET;
    pcap_netaddr.sin_port = htons(PORT_LISTEN_PCAP);
    pcap_netaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Setup the pcap socket
    DIE_ON_ERROR(server_setup(
        &listen_fd_pcap_, &pcap_netaddr,
        num_nodes, true)); // Setup pcap server

    // Spawn thread to accept new connections
    accept_args args(listen_fd_pcap_, true,
            timeout_connect_ms_, num_nodes);

    std::thread accept_thread(server_accept,
                              std::ref(args));
    while (!args.started) {}

    // Send a message to every fragment to setup the pcap overlay
    DIE_DURING_ACCEPT(foreach_fragment_send_ctrl(message::type::
        SETUP_PCAP, [this, pcap_netaddr] (const uint16_t, message& m) {

        auto payload = m.payload<message::request::setup_pcap>();
        payload->pcap_netaddr = pcap_netaddr;
    }));

    // Wait for every fragment to acknowledge pcap overlay setup
    DIE_DURING_ACCEPT(foreach_fragment_recv_ctrl(message::type::SETUP_PCAP,
        [this] (const uint16_t, const message&) { return error_code::NONE; }));

    accept_thread.join();
    if (args.rc != 0) { // Exit on error
        return error_code::SOCKET_ACCEPT_FAILED;
    }
    else if (args.num_accepted != num_nodes) {
        return error_code::SOCKET_ACCEPT_TIMEOUT;
    }

    // Next, wait for fragments to perform handshakes on the
    // pcap overlay. We will use fragments' messages to map
    // local pcap fds (returned by accept) to fragment IDs.
    std::vector<int> fds;
    for (size_t idx = 0; idx < fragments_.size(); idx++) {
        fds.push_back(args.states[idx].connection_fd);
    }

    auto lambda = [this, fds] (
        const uint16_t idx, const message& m) {
        const uint16_t fragment_id = m.get_fragment_id();

        // Ensure that the fragment ID is valid
        if (fragment_id >= fragments_.size()) {
            return error_code::BAD_FRAGMENT_ID;
        }
        // No two fragments must claim to have the same ID
        else if (fragments_[fragment_id].fd_pcap != -1) {
            return error_code::BAD_FRAGMENT_ID;
        }
        fragments_[fragment_id].fd_pcap = fds[idx];
        return error_code::NONE;
    };
    // Test connectivity on the pcap overlay
    DIE_ON_ERROR(foreach_fragment_recv_generic(
        false, fds, message::type::SETUP_PCAP,
        lambda, error_code::PCAP_CONNECTION_BROKEN));

    // Complete handshake
    for (size_t fid = 0; fid < fragments_.size(); fid++) {
        fds[fid] = fragments_[fid].fd_pcap;
    }
    return foreach_fragment_send_generic(
        fds, message::type::SETUP_PCAP,
        [this] (const uint16_t, message&) {},
        error_code::PCAP_CONNECTION_BROKEN);
}
// Cleanup
#undef DIE_DURING_ACCEPT

error_code
orchestrator::run_state_create_topology() {
    assert(state_ == state_t::CREATE_MIXNET_TOPOLOGY);
    auto error_code = error_code::NONE; // Return value
    const auto& topology = testcase_->get_graph().topology();

    auto send_lambda = [this, topology] (const uint16_t idx, message& msg) {
        auto payload = msg.payload<message::request::topology>();
        const auto& node = testcase_->get_graph().get_node(idx);

        // Populate the message payload
        payload->mixaddr = node.mixaddr();
        payload->num_neighbors = topology[idx].size();

        // Mixnet node configuration
        payload->mixing_factor = node.mixing_factor();
        payload->do_random_routing = node.do_random_routing();
        payload->reelection_interval_ms = testcase_->reelection_interval_ms();
        payload->root_hello_interval_ms = testcase_->root_hello_interval_ms();

        for (uint16_t nid = 0; nid < topology[idx].size(); nid++) {
            payload->link_costs()[nid] = node.link_costs()[nid];
        }
    };
    // Send the message to every fragment
    DIE_ON_ERROR(foreach_fragment_send_ctrl(
        message::type::TOPOLOGY, send_lambda));

    // Wait for acknowledgement
    return foreach_fragment_recv_ctrl(message::type::TOPOLOGY, [this]
        (const uint16_t, const message&) { return error_code::NONE; });
}

error_code
orchestrator::run_state_start_mixnet_server() {
    assert(state_ == state_t::START_ALL_MIXNET_SERVERS);
    auto error_code = error_code::NONE; // Return value
    const auto& topology = testcase_->get_graph().topology();

    // Send the message to every fragment
    DIE_ON_ERROR(foreach_fragment_send_ctrl(
        message::type::START_MIXNET_SERVER, [
            this] (const uint16_t, message&) {}));

    auto lambda = [this, topology] (const uint16_t idx,
                                    const message& m) {
        auto payload = m.payload<message::
            response::start_mixnet_server>();

        // No neighbors, nothing to do
        if (topology[idx].empty()) {
            return error_code::NONE;
        }
        // Invalid server address
        else if ((payload->server_netaddr.sin_family != AF_INET) ||
                 (payload->server_netaddr.sin_addr.s_addr == 0) ||
                 (payload->server_netaddr.sin_port == 0)) {
            return error_code::FRAGMENT_INVALID_SADDR;
        }
        sockaddr_in baseaddr; // Base address
        socklen_t addrlen = sizeof(sockaddr_in);
        if (getpeername(fragments_[idx].fd_ctrl,
            (sockaddr*) &baseaddr, &addrlen) != 0) {
            return error_code::SOCKET_OPTIONS_FAILED;
        }
        fragments_[idx].mixnet_server_netaddr = (
            payload->server_netaddr);

        fragments_[idx].mixnet_server_netaddr.
            sin_addr.s_addr = baseaddr.sin_addr.s_addr;

        return error_code::NONE;
    };
    // Wait for acknowledgement
    return foreach_fragment_recv_ctrl(
        message::type::START_MIXNET_SERVER, lambda);
}

error_code
orchestrator::run_state_start_mixnet_clients() {
    assert(state_ == state_t::START_ALL_MIXNET_CLIENTS);
    auto error_code = error_code::NONE; // Return value
    const auto& topology = testcase_->get_graph().topology();

    auto send_lambda = [this, topology] (const uint16_t idx, message& m) {
        auto payload = m.payload<message::request::start_mixnet_clients>();

        // Populate the message payload
        payload->num_neighbors = topology[idx].size();
        auto server_netaddrs = payload->neighbor_server_netaddrs();
        for (size_t nid = 0; nid < topology[idx].size(); nid++) {
            server_netaddrs[nid] = fragments_[
                topology[idx][nid]].mixnet_server_netaddr;
        }
    };
    // Send the message to every fragment
    DIE_ON_ERROR(foreach_fragment_send_ctrl(
        message::type::START_MIXNET_CLIENTS, send_lambda));

    auto recv_lambda = [this, topology] (const uint16_t idx, const message& m) {
        auto payload = m.payload<message::response::start_mixnet_clients>();

        // Incorrect neighbor count
        if (payload->num_neighbors != topology[idx].size()) {
            return error_code::FRAGMENT_BAD_NEIGHBOR_COUNT;
        }
        else {
            auto client_netaddrs = payload->client_netaddrs();
            for (size_t nid = 0; nid < topology[idx].size(); nid++) {
                // Invalid client address
                if ((client_netaddrs[nid].sin_family != AF_INET) ||
                    (client_netaddrs[nid].sin_addr.s_addr == 0) ||
                    (client_netaddrs[nid].sin_port == 0)) {
                    return error_code::FRAGMENT_INVALID_CADDR;
                }
                fragments_[idx].mixnet_client_netaddrs.push_back(
                    client_netaddrs[nid]);
            }
        }
        return error_code::NONE;
    };
    // Wait for acknowledgement
    return foreach_fragment_recv_ctrl(
        message::type::START_MIXNET_CLIENTS, recv_lambda);
}

error_code
orchestrator::run_state_resolve_mixnet_connections() {
    assert(state_ == state_t::RESOLVE_MIXNET_CONNECTIONS);
    const auto& topology = testcase_->get_graph().topology();
    auto error_code = error_code::NONE; // Return value

    auto lambda = [this, topology] (const uint16_t idx, message& m) {
        auto payload = m.payload<message::
            request::resolve_mixnet_connections>();

        // For each neighbor, find the client netaddress
        // it uses to communicate with this Mixnet node.
        payload->num_neighbors = topology[idx].size();
        for (size_t i = 0; i < topology[idx].size(); i++) {

            bool success = false;
            uint16_t fragment_id = topology[idx][i];
            auto client_netaddrs = payload->neighbor_client_netaddrs();
            for (size_t j = 0; j < topology[fragment_id].size(); j++) {
                // This node is {fragment_id}'s j'th neighbor
                if (topology[fragment_id][j] == idx) {
                    client_netaddrs[i] = fragments_[
                        fragment_id].mixnet_client_netaddrs[j];

                    success = true; break;
                }
            }
            // Sanity check: Ensure that each pair of nodes
            // has a consistent adjacency relationship.
            assert(success);
        }
    };
    // Send the message to every fragment
    DIE_ON_ERROR(foreach_fragment_send_ctrl(
        message::type::RESOLVE_MIXNET_CONNS, lambda));

    // Wait for acknowledgement
    return foreach_fragment_recv_ctrl(
        message::type::RESOLVE_MIXNET_CONNS, [
            this] (const uint16_t, const message&) {
                        return error_code::NONE; });
}

error_code
orchestrator::run_state_start_testcase() {
    assert(state_ == state_t::START_TESTCASE);

    auto error_code = error_code::NONE;
    DIE_ON_ERROR(foreach_fragment_send_ctrl(
        message::type::START_TESTCASE, [this] (
                const uint16_t, message&) {}));

    // Wait for acknowledgement
    return foreach_fragment_recv_ctrl(
        message::type::START_TESTCASE, [this]
            (const uint16_t, const message&) {
                return error_code::NONE; });
}

error_code
orchestrator::run_state_end_testcase() {
    assert(state_ == state_t::END_TESTCASE);

    auto error_code = error_code::NONE;
    DIE_ON_ERROR(foreach_fragment_send_ctrl(
        message::type::END_TESTCASE, [this] (
                const uint16_t, message&) {}));

    // Wait for acknowledgement
    return foreach_fragment_recv_ctrl(
        message::type::END_TESTCASE, [this] (
            const uint16_t, const message&) {
                return error_code::NONE; });
}

error_code
orchestrator::run_state_graceful_shutdown() {
    assert(state_ == state_t::GRACEFUL_SHUTDOWN);

    // Send the message to every fragment
    foreach_fragment_send_ctrl(message::type::SHUTDOWN, [this] (
                               const uint16_t, message&) {});

    auto lambda = [this] (const uint16_t idx, const message&) {
        fragments_[idx].pid = -1; return error_code::NONE;
    };
    // Wait for acknowledgement
    return foreach_fragment_recv_ctrl(
        message::type::SHUTDOWN, lambda);
}

/**
 * FSM functionality: ORCHESTRATOR methods.
 */
void orchestrator::run_state_forceful_shutdown() {
    assert(state_ == state_t::FORCEFUL_SHUTDOWN);

    // If graceful shutdown was unsuccessful, we don't know what
    // state the fragments are in. As such, we issue SIGKILL for
    // all fragment processes.
    destroy_fragments(SIGKILL);
}

void orchestrator::run_state_reset() {
    // Reset the internal state
    assert(state_ == state_t::RESET);
    destroy_sockets(); // Note: Sockets must be destroyed first
                       // before wiping other networking state!

    pcap_thread_error_ = error_code::NONE;
    pcap_thread_run_ = true;

    listen_fd_ctrl_ = -1;
    listen_fd_pcap_ = -1;
    fragments_.clear();
}

/**
 * Pcap loop.
 */
void orchestrator::pcap_thread_loop() {
    using namespace networking;
    auto error_code = error_code::NONE;
    const size_t num_fragments = fragments_.size();
    auto packet = msg_pcap_.payload<mixnet_packet>();

    std::unique_ptr<pollfd[]> pfds = (
        std::make_unique<pollfd[]>(num_fragments));

    for (size_t fid = 0; fid < num_fragments; fid++) {
        pfds[fid].fd = fragments_[fid].fd_pcap;
        pfds[fid].events = POLLIN;
        pfds[fid].revents = 0;
    }

    while (pcap_thread_run_ && (error_code == error_code::NONE)) {
        int rc = poll(pfds.get(), num_fragments, PCAP_POLL_TIMEOUT_MS);
        if (rc < 0) { error_code = error_code::POLL_ERROR; }

        for (size_t fid = 0; fid < num_fragments; fid++) {
            const bool is_subscribed = (
                fragments_[fid].is_pcap_subscribed);

            // Set up for next poll iteration
            pfds[fid].fd = (is_subscribed ?
                fragments_[fid].fd_pcap : -1);

            // Ignore any unsubscribed fragments
            if ((pfds[fid].revents != POLLIN) ||
                !is_subscribed) { continue; }

            // Attempt to receive a message (with small timeout)
            error_code = recv_with_timeout(fragments_[fid].fd_pcap,
                                           msg_pcap_, PCAP_POLL_TIMEOUT_MS,
                                           error_code::PCAP_CONNECTION_BROKEN);
            // Received a message
            if (error_code == error_code::NONE) {
                // Validate the message header
                error_code = check_header(msg_pcap_, true, fid,
                                          message::type::PCAP_DATA);

                if (error_code != error_code::NONE) { break; }
                if (packet->total_size > MAX_MIXNET_PACKET_SIZE) {
                    // We perform several layers of filtering for malformed
                    // packets before this, so really shouldn't reach here.
                    error_code = error_code::MIXNET_BAD_PACKET_SIZE;
                    break;
                }
                // Valid packet, invoke callback
                testcase_->pcap(msg_pcap_.get_fragment_id(),
                                msg_pcap_.payload<mixnet_packet>());
            }
            // Received zero messages, clear error. TODO(natre): This
            // should not really happen because of the preceding poll
            // call, so consider treating this as a real error.
            else if (error_code == error_code::RECV_ZERO_PENDING) {
                error_code = error_code::NONE;
            }
            // Irrecoverable error
            else { break; }
        }
        // Update the thread's error status
        pcap_thread_error_ = error_code;
    }
}

/**
 * Main loop.
 */
error_code orchestrator::run(testing::testcase& testcase) {
    assert(is_configured_); // Sanity check

    // Housekeeping
    bool done = false;
    testcase_ = &testcase;
    state_ = state_t::INIT;
    bool show_errors = true;
    auto next_state = state_t::INIT;
    error_code exit_code = error_code::NONE;

    // Run testcase setup
    testcase_->setup();

    while (!done) {
        auto error_code = error_code::NONE;
        switch (state_) {
        // Init
        case state_t::INIT: {
            error_code = run_state_init();
            next_state = (error_code == error_code::NONE) ?
                          state_t::SETUP_CTRL_PLANE :
                          state_t::FORCEFUL_SHUTDOWN;
        } break;

        // Setup ctrl overlay
        case state_t::SETUP_CTRL_PLANE: {
            error_code = run_state_setup_ctrl();
            next_state = (error_code == error_code::NONE) ?
                          state_t::SETUP_PCAP_PLANE :
                          state_t::FORCEFUL_SHUTDOWN;
        } break;

        // Setup pcap overlay
        case state_t::SETUP_PCAP_PLANE: {
            error_code = run_state_setup_pcap();
            next_state = (error_code == error_code::NONE) ?
                          state_t::CREATE_MIXNET_TOPOLOGY :
                          state_t::GRACEFUL_SHUTDOWN;
        } break;

        // Create test topology
        case state_t::CREATE_MIXNET_TOPOLOGY: {
            error_code = run_state_create_topology();
            next_state = (error_code == error_code::NONE) ?
                          state_t::START_ALL_MIXNET_SERVERS :
                          state_t::GRACEFUL_SHUTDOWN;
        } break;

        // Start server loops
        case state_t::START_ALL_MIXNET_SERVERS: {
            error_code = run_state_start_mixnet_server();
            next_state = (error_code == error_code::NONE) ?
                          state_t::START_ALL_MIXNET_CLIENTS :
                          state_t::GRACEFUL_SHUTDOWN;
        } break;

        // Start client loops
        case state_t::START_ALL_MIXNET_CLIENTS: {
            error_code = run_state_start_mixnet_clients();
            next_state = (error_code == error_code::NONE) ?
                          state_t::RESOLVE_MIXNET_CONNECTIONS :
                          state_t::GRACEFUL_SHUTDOWN;
        } break;

        // Start Mixnet connection resolution
        case state_t::RESOLVE_MIXNET_CONNECTIONS: {
            error_code = run_state_resolve_mixnet_connections();
            next_state = (error_code == error_code::NONE) ?
                          state_t::START_TESTCASE :
                          state_t::GRACEFUL_SHUTDOWN;
        } break;

        // Start test-case
        case state_t::START_TESTCASE: {
            error_code = run_state_start_testcase();
            next_state = (error_code == error_code::NONE) ?
                          state_t::RUN_TESTCASE :
                          state_t::END_TESTCASE;
        } break;

        // Run test-case
        case state_t::RUN_TESTCASE: {
            pcap_thread_ = std::thread(
                &orchestrator::pcap_thread_loop, this);

            error_code = testcase_->run(*this);
            pcap_thread_run_ = false;
            pcap_thread_.join();

            next_state = state_t::END_TESTCASE;
        } break;

        // End test-case
        case state_t::END_TESTCASE: {
            error_code = run_state_end_testcase();
            next_state = state_t::GRACEFUL_SHUTDOWN;
        } break;

        // Perform graceful shutdown
        case state_t::GRACEFUL_SHUTDOWN: {
            error_code = run_state_graceful_shutdown();
            next_state = (error_code == error_code::NONE) ?
                          state_t::RESET :
                          state_t::FORCEFUL_SHUTDOWN;
        } break;

        // Butcher everything
        case state_t::FORCEFUL_SHUTDOWN: {
            error_code = error_code::NONE;
            run_state_forceful_shutdown();
            next_state = state_t::RESET;
        } break;

        // Set up for the next test-case
        case state_t::RESET: {
            run_state_reset();
            done = true; // Finished run
            error_code = error_code::NONE;
        } break;

        // Unknown state
        default: { assert(false); }
        } // switch

        // Log the first error encountered and set the exit code
        if (show_errors && (error_code != error_code::NONE)) {
            std::cout << "[Orchestrator] Recorded error "
                      << static_cast<uint16_t>(error_code)
                      << " at state "
                      << static_cast<uint16_t>(state_)
                      << std::endl;

            exit_code = error_code;
            show_errors = false;
        }
        // Move to new state
        state_ = next_state;
    }
    if (exit_code == error_code::NONE) {
        std::cout << "[Orchestrator] Fragments returned OK" << std::endl;
    }
    std::cout << "[Orchestrator] Exiting normally" << std::endl;
    testcase_->teardown();
    return exit_code;
}

/**
 * Public API.
 */
void orchestrator::configure(const std::string& bin_dir,
                             const bool autotest_mode) {
    // Update configuration
    fragment_dir_ = bin_dir;
    autotest_mode_ = autotest_mode;

    // Use large timeouts in manual mode
    if (!autotest_mode_) {
        timeout_communication_ms_ = 5000; // 5 seconds
        timeout_connect_ms_ = 10 * 60 * 1000; // 10 minutes
    }
    is_configured_ = true;
}

error_code orchestrator::pcap_change_subscription(
    const uint16_t idx, const bool subscribe) {
    assert(state_ == state_t::RUN_TESTCASE);

    auto& current_subscription = (
        fragments_[idx].is_pcap_subscribed);

    if (current_subscription == subscribe) {
        // No change in subscription, return
        return error_code::NONE;
    }
    current_subscription = subscribe;
    // Lambda to populate the message payload
    auto lambda = [this, subscribe] (message& m) {
        auto payload = m.payload<message::
            request::pcap_subscription>();

        payload->subscribe = subscribe;
    };
    return fragment_request_response(
        idx, message::type::PCAP_SUBSCRIPTION, lambda);
}

error_code orchestrator::change_link_state(const uint16_t idx_a,
                                           const uint16_t idx_b,
                                           const bool is_enabled) {
    const auto& topology = testcase_->get_graph().topology();
    assert(state_ == state_t::RUN_TESTCASE);
    size_t b_nid_in_a = 0, a_nid_in_b = 0;
    size_t num_success = 0;

    assert((idx_a < topology.size()) && (idx_b < topology.size()));
    for (size_t nid = 0; nid < topology[idx_a].size(); nid++) {
        if (topology[idx_a][nid] == idx_b) {
            b_nid_in_a = nid; num_success++;
            break;
        }
    }
    for (size_t nid = 0; nid < topology[idx_b].size(); nid++) {
        if (topology[idx_b][nid] == idx_a) {
            a_nid_in_b = nid; num_success++;
            break;
        }
    }
    if (num_success != 2) {
        std::cout << "[Orchestrator] Improper adjacency relationship "
                  << "between nodes " << idx_a << " and " << idx_b
                  << ", please check topology" << std::endl;

        return error_code::BAD_TESTCASE;
    }
    // Lambdas to populate the payloads
    auto lambda_a = [this, b_nid_in_a, is_enabled] (message& m) {
        auto payload = m.payload<message::
            request::change_link_state>();

        payload->neighbor_id = b_nid_in_a;
        payload->state = is_enabled;
    };
    auto lambda_b = [this, a_nid_in_b, is_enabled] (message& m) {
        auto payload = m.payload<message::
            request::change_link_state>();

        payload->neighbor_id = a_nid_in_b;
        payload->state = is_enabled;
    };
    auto error_code = error_code::NONE;
    DIE_ON_ERROR(fragment_request_response(idx_a,
        message::type::CHANGE_LINK_STATE, lambda_a));

    return fragment_request_response(idx_b,
        message::type::CHANGE_LINK_STATE, lambda_b);
}

error_code
orchestrator::send_packet(const uint16_t src_idx,
                          const uint16_t dst_idx,
                          const mixnet_packet_type_t type,
                          const std::string data_string) {
    assert(state_ == state_t::RUN_TESTCASE);
    if (data_string.size() > MAX_MIXNET_DATA_SIZE) {
        std::cout << "[Orchestrator] Payload data should be "
                  << "smaller than " << MAX_MIXNET_DATA_SIZE
                  << " bytes" << std::endl;

        return error_code::BAD_TESTCASE;
    }

    // Lambda to populate the message payload
    auto lambda = [this, src_idx, dst_idx,
        type, data_string] (message& m) {
        auto payload = m.payload<
            message::request::send_packet>();

        payload->type = type;
        payload->src_mixaddr = (testcase_->
            get_graph().get_node(src_idx).mixaddr());

        payload->dst_mixaddr = (testcase_->
            get_graph().get_node(dst_idx).mixaddr());

        memcpy(payload->data(), data_string.c_str(),
               data_string.size());

        payload->data_length = data_string.size();
    };
    return fragment_request_response(
        src_idx, message::type::SEND_PACKET, lambda);
}

/**
 * Constructor.
 */
orchestrator::orchestrator() {
    // Register signal handler
    signal(SIGPIPE, SIG_IGN);

    // Randomness
    srand(time(NULL));
}

// Cleanup
#undef DIE_ON_ERROR

} // namespace framework
