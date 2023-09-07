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
#ifndef TESTING_COMMON_TESTCASE_H_
#define TESTING_COMMON_TESTCASE_H_

#include "graph.h"
#include "framework/error.h"
#include "mixnet/packet.h"

#include <assert.h>
#include <memory>
#include <string>

// Forward declaration
namespace framework { class orchestrator; }

namespace testing {

// Helper macros
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                  \
    TypeName(const TypeName&) = delete;                     \
    void operator=(const TypeName&) = delete

/**
 * Abstract base class representing a generic test-case.
 */
class testcase {
protected:
    // Internal state
    bool pass_pcap_ = true;                     // Pass pcap checks?
    bool pass_teardown_ = false;                // Pass teardown checks?
    std::unique_ptr<graph> graph_{};            // Underlying test graph
    framework::error_code error_ = (            // Running test error code
        framework::error_code::NONE);

    // Test results
    uint64_t pcap_count_ = 0;                   // RX packet count

    // Configuration
    uint32_t root_hello_interval_ms_ = 2000;    // Default: 2 seconds
    uint32_t reelection_interval_ms_ = 20000;   // Default: 20 seconds

    /**
     * Internal helper methods.
     */
    void init_graph(const uint16_t num_nodes);

    DISALLOW_COPY_AND_ASSIGN(testcase);
    explicit testcase(const std::string& name) : name(name) {}

public:
    // Testcase name
    const std::string name;

    // Virtual, do not touch!
    virtual ~testcase() {}

    // Accessors
    uint64_t pcap_count() const { return pcap_count_; }
    bool is_pass() const { return (pass_pcap_ && pass_teardown_); }
    uint32_t root_hello_interval_ms() const { return root_hello_interval_ms_; }
    uint32_t reelection_interval_ms() const { return reelection_interval_ms_; }
    const graph& get_graph() const { assert(graph_ != nullptr); return *graph_; }

    /**
     * Orchestration methods.
     */
    // Callback invoked when a pcap subscription is triggered.
    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) = 0;

    // Callback invoked at the very beginning of orchestrator::
    // run(). ALL static configuration (e.g., initializing the
    // graph and setting timeouts) must be performed here.
    virtual void setup() = 0;

    // Callback invoked once the testcase commences.
    virtual framework::error_code run(framework::
                    orchestrator& orchestrator) = 0;

    // Callback invoked at the very end of orchestrator::run(),
    // after the testcase completes. This should be used to set
    // the testcase::pass_teardown_ field.
    virtual void teardown() = 0;

    /**
     * Entry-point for all testcases.
     */
    static int run_testcase(testcase& tc, int argc, char **argv);
};

// Cleanup
#undef DISALLOW_COPY_AND_ASSIGN

} // namespace testing

#endif // TESTING_COMMON_TESTCASE_H_
