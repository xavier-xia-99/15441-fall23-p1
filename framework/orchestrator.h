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
#ifndef FRAMEWORK_ORCHESTRATOR_H_
#define FRAMEWORK_ORCHESTRATOR_H_

#include "error.h"
#include "message.h"
#include "mixnet/address.h"

#include <functional>
#include <string>
#include <thread>
#include <vector>

// Forward declaration
namespace testing { class testcase; }

namespace framework {

// Helper macros
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                  \
    TypeName(const TypeName&) = delete;                     \
    void operator=(const TypeName&) = delete

/**
 * Central controller orchestrating distributed fragments.
 */
class orchestrator final {
public:
    // Orchestrator's ctrl port number
    static constexpr uint16_t PORT_LISTEN_CTRL = 9107;
    // Orchestrator's pcap port number
    static constexpr uint16_t PORT_LISTEN_PCAP = 9108;
    // Poll timer for quasi-blocking pcap communication
    static constexpr uint64_t PCAP_POLL_TIMEOUT_MS = 50;
    // Wait time to send/recv data to/from all fragments
    static constexpr uint64_t DEFAULT_WAIT_TIME_MS = 5000;

private:
    // FSM states. These represent common tasks that need to
    // be run for every testcase (set up the mixnet topology,
    // shut down fragments, etc.).
    enum class state_t {
        INIT = 0,
        SETUP_CTRL_PLANE,
        SETUP_PCAP_PLANE,
        CREATE_MIXNET_TOPOLOGY,
        START_ALL_MIXNET_SERVERS,
        START_ALL_MIXNET_CLIENTS,
        RESOLVE_MIXNET_CONNECTIONS,
        START_TESTCASE,
        RUN_TESTCASE,
        END_TESTCASE,
        GRACEFUL_SHUTDOWN,
        FORCEFUL_SHUTDOWN,
        RESET,
    };

    // Fragment metadata
    struct fragment_metadata {
        int pid = -1;                                           // Fragment process ID
        int fd_ctrl = -1;                                       // Local ctrl socket FD
        int fd_pcap = -1;                                       // Local pcap socket FD

        volatile bool is_pcap_subscribed = false;               // Get pcap data?
        sockaddr_in mixnet_server_netaddr{};                    // Network address of this
                                                                // fragment's Mixnet server

        // Ordered list of Mixnet client network addresses. The i'th element
        // corresponds to the network address on which this Mixnet node will
        // receive data from its i'th neighbor.
        std::vector<sockaddr_in> mixnet_client_netaddrs;
    };
    // Maps fragment IDs to metadata
    std::vector<fragment_metadata> fragments_;

    // State for managing the pcap overlay
    std::thread pcap_thread_;
    volatile bool pcap_thread_run_ = true;
    error_code pcap_thread_error_ = error_code::NONE;

    // Housekeeping
    message msg_ctrl_{};                                        // Message buffer (ctrl)
    message msg_pcap_{};                                        // Message buffer (pcap)
    int listen_fd_ctrl_ = -1;                                   // Server ctrl socket FD
    int listen_fd_pcap_ = -1;                                   // Server pcap socket FD
    state_t state_ = state_t::INIT;                             // The current FSM state
    testing::testcase *testcase_ = nullptr;                     // Pointer to current testcase

    // Configuration
    bool is_configured_ = false;                                // Configuration complete?
    std::string fragment_dir_{};                                // Fragment executable path
    bool autotest_mode_ = false;                                // Use the autotester mode?
    uint64_t timeout_connect_ms_ = DEFAULT_WAIT_TIME_MS;        // Setup connection timeout
    uint64_t timeout_communication_ms_ = DEFAULT_WAIT_TIME_MS;  // Regular send/recv timeout

    /**
     * FSM functionality.
     */
    error_code run_state_init();
    error_code run_state_setup_ctrl();
    error_code run_state_setup_pcap();
    error_code run_state_end_testcase();
    error_code run_state_start_testcase();
    error_code run_state_create_topology();
    error_code run_state_graceful_shutdown();
    error_code run_state_start_mixnet_server();
    error_code run_state_start_mixnet_clients();
    error_code run_state_resolve_mixnet_connections();

    // Orchestrator methods
    void run_state_reset();
    void run_state_forceful_shutdown();

    /**
     * Miscellaneous helper methods.
     */
    void destroy_sockets();
    void pcap_thread_loop();
    void destroy_fragments(int signal);

    void prepare_header(message& msg,
                        const uint16_t fid,
                        const message::type type);

    error_code check_header(const message& msg,
        const bool check_id, const uint16_t id,
        const message::type type);

    error_code send_with_timeout(const int fd,
        const message& m, const uint64_t time,
        const error_code connection_error);

    error_code recv_with_timeout(const int fd,
        message& m, const uint64_t timeout_ms,
        const error_code connection_error);

    error_code fragment_request_response(
        const uint16_t fid, const message::type type,
        const std::function<void(message&)>& lambda);

    error_code foreach_fragment_send_ctrl(
        const message::type type, const std::function<
                void(const uint16_t, message&)>& lambda);

    error_code foreach_fragment_send_generic(
        const std::vector<int>& fds, const message::type type,
        const std::function<void(const uint16_t, message&)>&
            lambda, const error_code connection_error);

    error_code foreach_fragment_recv_ctrl(
        const message::type type,
        const std::function<error_code(
            const uint16_t, const message&)>& lambda);

    error_code foreach_fragment_recv_generic(
        const bool check_fragment_ids, const std::vector<int>& fds,
        const message::type type, const std::function<error_code(
            const uint16_t, const message&)>& lambda,
            const error_code connection_error);

public:
    explicit orchestrator();
    DISALLOW_COPY_AND_ASSIGN(orchestrator);

    // Test API
    /**
     * Configure the orchestrator with command-line arguments (e.g., server
     * address, autotest mode). Must be invoked before orchestrator::run().
     */
    void configure(const std::string& bin_dir, const bool autotest_mode);

    /**
     * Main orchestrator method. Once the virtual topology is set up and all
     * the nodes are running, passes control to the callback for the current
     * test-case (testcase::run()). Receiving a packet that the orchestrator
     * subscribes to invokes the testcase::pcap() method. Note: This runs in
     * a separate thread, so careful with shared state!
     */
    error_code run(testing::testcase& testcase);

    /**
     * The methods that appear after this point are run-time configuration
     * parameters. They must be invoked AFTER control passes to testcase::
     * run() (i.e., only while the test-case is running).
     */
    // Allows the orchestrator to "subscribe" to packet capture traffic.
    // Any packets that appear on the output (mixnet_send() to the user)
    // of a subscribed node will be sent back to the orchestrator and
    // will invoke the registered pcap callback.
    error_code pcap_change_subscription(
        const uint16_t idx, const bool subscribe);

    // Enable/disable the link between two nodes
    error_code change_link_state(const uint16_t idx_a,
                                 const uint16_t idx_b,
                                 const bool is_enabled);

    // Send a packet with source and destination addresses corresponding
    // to src_idx and dst_idx, respectively. The optional data_string
    // parameter allows you to specify the data to send.
    error_code send_packet(const uint16_t src_idx,
                           const uint16_t dst_idx,
                           const mixnet_packet_type_t type,
                           const std::string data_string="");
};

// Cleanup
#undef DISALLOW_COPY_AND_ASSIGN

} // namespace framework

#endif // FRAMEWORK_ORCHESTRATOR_H_
