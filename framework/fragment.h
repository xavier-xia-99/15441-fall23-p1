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
#ifndef FRAMEWORK_FRAGMENT_H_
#define FRAMEWORK_FRAGMENT_H_

#include "error.h"
#include "message.h"
#include "networking.h"
#include "mixnet/address.h"
#include "mixnet/config.h"
#include "external/itc/message_queue.h"

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <stdint.h>
#include <thread>
#include <vector>

namespace framework {

// Helper macros
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                  \
    TypeName(const TypeName&) = delete;                     \
    void operator=(const TypeName&) = delete

/**
 * Represents a fragment (container for a Mixnet node).
 */
class fragment final {
public:
    // Constant parameters
    static constexpr uint32_t MQ_PCAP_DEPTH = 128;
    static constexpr uint32_t MQ_USER_DEPTH = 128;
    static constexpr uint64_t DEFAULT_TIMEOUT_MS = 5000;
    static constexpr uint16_t INVALID_FRAGMENT_ID = (-1);

    /**
     * Represents per-thread state.
     */
    struct thread_state {
        // Custom exception (enables arbitrary thread exit points)
        class exit_exception : public std::exception {};

        volatile bool exited = false;                       // Whether this thread has exited
        volatile bool started = false;                      // Whether the thread has started
        volatile bool keep_running = true;                  // Continue running this thread?
        error_code exit_code = error_code::NONE;            // Thread's return code
    };

    /**
     * Represents a node's private context.
     */
    struct node_context {
    private:
        // Mixnet node configuration
        mixnet_node_config config{};                        // This node's configuration
        // TX
        int tx_listen_fd = -1;                              // Listen FD (this node as server)
        std::vector<int> tx_socket_fds;                     // Socket FDs (this node as server)
        sockaddr_in tx_server_netaddr{};                    // This node's local server address
        // RX
        uint16_t rx_port_idx = 0;                           // Next port to serve (round-robin)
        std::vector<int> rx_socket_fds;                     // Socket FDs (this node as client)
        std::unique_ptr<std::mutex[]> port_mutexes;         // Mutexes guarding RX socket state
        std::vector<sockaddr_in> neighbor_netaddrs;         // Server addrs of neighboring nodes
        // ITC
        thread_state ts{};                                  // Thread state
        message_queue& mq_pcap;                             // MQ for pcap data
        message_queue& mq_user;                             // MQ for user-injected packets
        // Miscellaneous
        std::vector<bool> link_states;                      // NID -> Link state (up: true)
        std::unique_ptr<char[]> recv_buffer{};              // Scratch receive packet buffer
        volatile bool is_pcap_subscribed = false;           // Orchestrator subscribed for pcap?

        /**
         * Helper methods.
         */
        error_code _recv_once(const int fd, char *const buffer);
        error_code _send_blocking(const int fd, const char *const buffer);

    public:
        ~node_context();
        DISALLOW_COPY_AND_ASSIGN(node_context);
        explicit node_context(message_queue& mq_pcap,
                              message_queue& mq_user);

        int node_send(const uint8_t port, mixnet_packet *const packet);
        int node_recv(uint8_t *const port, mixnet_packet **const packet);

        // Expose internal state
        friend class fragment;
    };

private:
    // FSM states. These represent common tasks that need to
    // be run for every testcase (set up the mixnet topology,
    // shut down fragments, etc.).
    enum class state_t {
        SETUP_CTRL = 0,
        SETUP_PCAP,
        CREATE_TOPOLOGY,
        START_MIXNET_SERVER,
        START_MIXNET_CLIENT,
        RESOLVE_MIXNET_CONNECTIONS,
        START_TESTCASE,
        RUN_TESTCASE,
        SHUTDOWN,
        DONE,
    };

    // Fragment state
    uint16_t fid_ = -1;                                     // Fragment's unique ID
    bool autotest_mode_ = false;                            // Fragment in autotest mode?
    state_t state_ = state_t::SETUP_CTRL;                   // Fragment's current FSM state

    // Networking
    message msg_ctrl_{};                                    // Message buffer (ctrl)
    message msg_pcap_{};                                    // Message buffer (pcap)
    int local_fd_ctrl_ = -1;                                // Local FD for ctrl overlay
    int local_fd_pcap_ = -1;                                // Local FD for pcap overlay
    sockaddr_in orc_netaddr_{};                             // Orchestrator ctrl netaddr
    uint64_t timeout_connect_long_ = DEFAULT_TIMEOUT_MS;    // Long timeout for connection setup
    uint64_t timeout_connect_short_ = DEFAULT_TIMEOUT_MS;   // Short timeout for connection setup

    // Node context
    std::unique_ptr<node_context> node_context_{};

    // MQs and thread state
    thread_state ts_pcap_{};                                // Thread state for pcap
    std::thread thread_node_;                               // Thread running node impl
    std::thread thread_pcap_;                               // Thread handling pcap plane
    std::unique_ptr<message_queue> mq_pcap_{};              // MQ for packet capture data
    std::unique_ptr<message_queue> mq_user_{};              // MQ for user-injected packets

    // Temporary FSM state
    std::thread node_accept_thread_;                        // Thread for accepting connections
    std::unique_ptr<networking::accept_args>                // Args for node's accept invocation
                        node_accept_args_{};
    /**
     * Miscellaneous helper methods.
     */
    void worker_node(); // Run node implementation
    void worker_pcap(); // Run thread handling the pcap stream

    void destroy_node_context();
    void init_node_context(message::request::topology *const p);

    void prepare_header(message& m, const message::type type,
                        const error_code error_code) const;

    error_code check_header(
        const bool check_id, const bool check_type,
        const message& m, const message::type type) const;

    static error_code _send_blocking(
        const int fd, const message& m, const error_code e);

    static error_code _recv_blocking(
        const int fd, message& msg, const error_code e);

    error_code send_response(const bool is_ctrl,
        const message::type type, const error_code ec,
        const std::function<error_code(message&)>& lambda);

    error_code recv_request(const bool is_ctrl, const bool check_id,
        const bool check_type, const message::type type, const std::
                        function<error_code(const message&)>& lambda);

    /**
     * FSM functionality.
     */
    error_code run_state_setup_ctrl();
    error_code run_state_setup_pcap();
    error_code run_state_do_shutdown();
    error_code run_state_run_testcase();
    error_code run_state_start_testcase();
    error_code run_state_create_topology();
    error_code run_state_start_mixnet_server();
    error_code run_state_start_mixnet_client();
    error_code run_state_resolve_mixnet_connections();
    /**
     * Fragment testcase tasks.
     */
    error_code task_end_testcase();
    error_code task_update_pcap_subscription(const bool subscribe);
    error_code task_send_packet(message::request::send_packet *const p);
    error_code task_update_link_state(const uint16_t nid, const bool state);

public:
    ~fragment();
    DISALLOW_COPY_AND_ASSIGN(fragment);
    explicit fragment(const sockaddr_in& orc_netaddr);

    // Public interface
    void run();
};

// Cleanup
#undef DISALLOW_COPY_AND_ASSIGN

} // namespace framework

#endif // FRAMEWORK_FRAGMENT_H_
