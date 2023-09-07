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
#ifndef FRAMEWORK_MESSAGE_H_
#define FRAMEWORK_MESSAGE_H_

#include "error.h"
#include "mixnet/address.h"
#include "mixnet/packet.h"

#include <netinet/in.h>
#include <stdint.h>

namespace framework {

// Helper macros
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                  \
    TypeName(const TypeName&) = delete;                     \
    void operator=(const TypeName&) = delete

#define CHECK_SIZE_VLA_PTR_ALIGN(typename)                  \
    static_assert((sizeof(typename) % 8) == 0, "Bad size")

#define GENERATE_POD_LENGTH_DEFN(typename)                  \
    static constexpr length_t length() {                    \
        return sizeof(typename);                            \
    }

/**
 * Represents a framework message.
 */
class message final {
public:
    // Super-secret canary (sentinel value)
    static const uint32_t canary = 0xdeadbeef;

    /**
     * Framework message types.
     *
     * 16-bit value representing the type of message enclosed. The most
     * siginificant bit indicates the message direction (0 for requests
     * going from the orchestrator to fragments, 1 for responses in the
     * reverse direction).
     *
     * Important: If you add another message type here, you must update
     * compute_total_length() to perform RTTI-based computation for the
     * corresponding type.
     */
    enum class type {
        NOOP = 0,                               // No operation (DO NOT TOUCH)

        SETUP_CTRL,                             // Setup ctrl overlay
        SETUP_PCAP,                             // Setup pcap overlay
        TOPOLOGY,                               // Create the topology
        START_MIXNET_SERVER,                    // Start Mixnet server
        START_MIXNET_CLIENTS,                   // Start Mixnet clients
        RESOLVE_MIXNET_CONNS,                   // Resolve Mixnet connections
        CHANGE_LINK_STATE,                      // Change network link states
        PCAP_DATA,                              // Fragment-captured pcap data
        PCAP_SUBSCRIPTION,                      // Change subscription to pcaps
        SEND_PACKET,                            // Send a packet on the network
        START_TESTCASE,                         // Indicate testcase commencing
        END_TESTCASE,                           // Indicate testcase completion
        SHUTDOWN,                               // Teardown the fragment process

        ENUMERATION_LIMIT,                      // Sentinel value (DO NOT TOUCH)
    };
    typedef uint16_t type_t; // Shorter type alias
    static_assert((1 << ((sizeof(type_t) * 8) - 1)) >
                  static_cast<uint64_t>(type::ENUMERATION_LIMIT),
                  "Type alias for message::type does not span enum's range.");

    // Message sizing
    typedef uint16_t length_t;

    /**
     * Message header structure. This is a common header for
     * all messages exchanged on the ctrl and pcap overlays.
     */
    struct header {
        length_t total_length;                  // Total message len (bytes)
        uint16_t message_code;                  // Message polarity and type
        uint16_t fragment_id;                   // Unique ID of the fragment
        uint16_t error_code;                    // 0 on success, else error (error.h)
    };
    static_assert(sizeof(header) == 8, "message::header has bad size");
    static_assert(sizeof(header::total_length) == sizeof(mixnet_packet::total_size));

    static_assert((1 << (sizeof(uint16_t) * 8)) >
                  static_cast<uint16_t>(error_code::ENUMERATION_LIMIT));

    // Size-related typedefs
    static const length_t MIN_MESSAGE_LENGTH = (
        sizeof(header) + sizeof(canary));

    static const length_t MAX_MESSAGE_LENGTH = (
        (1 << (sizeof(message::length_t) * 8)) - 1);

    ~message();
    explicit message();
    DISALLOW_COPY_AND_ASSIGN(message);

    // Accessors
    type get_type() const;
    bool validate() const;
    bool is_request() const;
    uint16_t get_fragment_id() const;
    length_t get_total_length() const;
    error_code get_error_code() const;
    length_t compute_total_length() const;

    // Mutators
    void clear();
    void finalize();
    void set_fragment_id(const uint16_t fragment_id);
    void set_error_code(const error_code error_code);
    void set_code(const bool is_request, const type type);

    // Internal buffers
    char *buffer() const { return buffer_; }
    header *get_header() const { return reinterpret_cast<header*>(buffer_); }
    template<typename T> T *payload() const { return reinterpret_cast<T*>(
                                              buffer_ + sizeof(header)); }
private:

    // Internal buffer
    char *buffer_ = nullptr;

    // Internal helper methods
    static type code_to_type(const uint16_t code);
    static bool code_is_request(const uint16_t code);
    static uint16_t code_create(const bool is_request, const type type);

public:
    /**
     * Payload structure for REQUEST messages.
     *
     * Note: Since C++ doesn't allow variable-length arrays (VLAs), we instead
     * use struct-specific member functions to compute the appropriate offsets
     * into the struct layout. This has 2 implications for these structs:
     *
     * 1. The length() method should be used to determine the actual size of a
     *    struct instance; sizeof() does not capture the size of the VLA field,
     *    and should NOT be used for this purpose.
     *
     * 2. Struct size computation (i.e., implementation of the length() method)
     *    may ultimately depend on another struct member (e.g., num_neighbors),
     *    which must be properly initialized before invoking length().
     */
    class request {
    public:
        // Setup ctrl overlay
        struct setup_ctrl {
            bool autotest_mode;                 // Use autotest mode?

            // Helper methods
            GENERATE_POD_LENGTH_DEFN(setup_ctrl)
        };
        // Setup pcap overlay
        struct setup_pcap {
            sockaddr_in pcap_netaddr{};         // Network address on which
                                                // pcap server is listening
            // Helper methods
            GENERATE_POD_LENGTH_DEFN(setup_pcap)
        };
        // Topology
        struct topology {
            mixnet_address mixaddr;             // Node's mixnet address
            uint16_t num_neighbors;             // Number of direct neighbors

            // Node configuration
            bool do_random_routing;             // Perform random routing?
            uint16_t mixing_factor;             // Mixing factor to use during routing
            uint32_t root_hello_interval_ms;    // Time between 'hello' messages
            uint32_t reelection_interval_ms;    // Time before starting reelection

            // NID -> Cost of routing on the link
            uint16_t *link_costs() {
                return reinterpret_cast<uint16_t*>(
                    reinterpret_cast<char*>(this) + sizeof(*this));
            }
            // Helper methods
            length_t length() const {
                return (sizeof(*this) +
                        (num_neighbors * sizeof(uint16_t)));
            }
        };
        CHECK_SIZE_VLA_PTR_ALIGN(topology);

        // Start Mixnet client
        struct start_mixnet_clients {
            uint16_t num_neighbors;             // Number of direct neighbors
            uint8_t padding_[6]{};              // Padding for pointer alignment

            // NID -> Netaddr of neighbor's server socket
            sockaddr_in *neighbor_server_netaddrs() {
                return reinterpret_cast<sockaddr_in*>(
                    reinterpret_cast<char*>(this) + sizeof(*this));
            }
            // Helper methods
            length_t length() const {
                return (sizeof(*this) +
                        (num_neighbors * sizeof(sockaddr_in)));
            }
        };
        CHECK_SIZE_VLA_PTR_ALIGN(start_mixnet_clients);

        // Resolve Mixnet network connections
        struct resolve_mixnet_connections {
            uint16_t num_neighbors;             // Number of direct neighbors
            uint8_t padding_[6]{};              // Padding for pointer alignment

            // NID -> Netaddr of neighbor's client socket
            sockaddr_in *neighbor_client_netaddrs() {
                return reinterpret_cast<sockaddr_in*>(
                    reinterpret_cast<char*>(this) + sizeof(*this));
            }
            // Helper methods
            length_t length() const {
                return (sizeof(*this) +
                        (num_neighbors * sizeof(sockaddr_in)));
            }
        };
        CHECK_SIZE_VLA_PTR_ALIGN(resolve_mixnet_connections);

        // Change network link state
        struct change_link_state {
            uint16_t neighbor_id;               // NID corresponding to the link to update
            bool state;                         // New state of the link (enabled if true)

            // Helper methods
            GENERATE_POD_LENGTH_DEFN(change_link_state)
        };
        // Change pcap subscription
        struct pcap_subscription {
            bool subscribe;                     // Whether to subscribe/unsubscribe
                                                // to/from the fragment's pcap data.
            // Helper methods
            GENERATE_POD_LENGTH_DEFN(pcap_subscription)
        };
        // Send a packet out over the network
        struct send_packet {
            mixnet_packet_type_t type;          // Packet type
            mixnet_address src_mixaddr;         // Source Mixnet address
            mixnet_address dst_mixaddr;         // Destination Mixnet address
            uint16_t data_length;               // Length of packet data field

            // Helper methods
            char *data() {
                return (reinterpret_cast<char*>(this) + sizeof(*this));
            }
            length_t length() const {
                return (sizeof(*this) +
                        (data_length * sizeof(char)));
            }
        };
        CHECK_SIZE_VLA_PTR_ALIGN(send_packet);
    };

    /**
     * Payload structure for RESPONSE messages.
     */
    class response {
    public:
        // Setup ctrl overlay
        struct setup_ctrl {
            int pid;                            // PID of fragment process

            // Helper methods
            GENERATE_POD_LENGTH_DEFN(setup_ctrl)
        };
        // Start Mixnet server
        struct start_mixnet_server {
            sockaddr_in server_netaddr{};       // Network address on which the
                                                // Mixnet server is listening.
            // Helper methods
            GENERATE_POD_LENGTH_DEFN(start_mixnet_server)
        };

        // Start Mixnet clients
        struct start_mixnet_clients {
            uint16_t num_neighbors;             // Number of direct neighbors
            uint8_t padding_[6]{};              // Padding for pointer alignment

            // NID -> Netaddr of client socket
            sockaddr_in *client_netaddrs() {
                return reinterpret_cast<sockaddr_in*>(
                    reinterpret_cast<char*>(this) + sizeof(*this));
            }
            // Helper methods
            length_t length() const { return (sizeof(*this) +
                                              (num_neighbors * sizeof(sockaddr_in))); }
        };
        CHECK_SIZE_VLA_PTR_ALIGN(start_mixnet_clients);
    };
};

// Cleanup
#undef GENERATE_POD_LENGTH_DEFN
#undef CHECK_SIZE_VLA_PTR_ALIGN
#undef DISALLOW_COPY_AND_ASSIGN

} // namespace framework

#endif // FRAMEWORK_MESSAGE_H_
