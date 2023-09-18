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
#include "testcase.h"

#include "framework/error.h"
#include "framework/orchestrator.h"
#include "external/argparse/argparse.hpp"

#include <chrono>
#include <iostream>
#include <filesystem>
#include <memory>
#include <string.h>
#include <thread>

namespace testing {

void testcase::init_graph(const uint16_t num_nodes) {
    graph_ = std::make_unique<graph>(num_nodes);
}

int testcase::run_testcase(testcase& tc, int argc, char **argv) {
    argparse::ArgumentParser program(tc.name);
    program.add_argument("-a")
           .default_value(false)
           .implicit_value(true)
           .help("Use autotest mode");
    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }
    // Parse arguments
    const bool autotest = (program["-a"] == true);
    // Assumes that test-cases are built in a separate subdirectory inside bin
    auto bin_dir = std::filesystem::path(argv[0]).parent_path().parent_path();

    // In autotest mode, the node processes all run on
    // localhost, so we use less conservative timers.
    if (autotest) {
        tc.root_hello_interval_ms_ = 10;
        tc.reelection_interval_ms_ = 100;
        tc.max_convergence_time_ms_ = 500;
        tc.max_propagation_time_ms_ = 500;
    }

    // Configure the orchestrator
    framework::orchestrator orchestrator;
    orchestrator.configure(bin_dir, autotest);
    std::cout << "[Testing] Starting " << tc.name << "..." << std::endl;

    // Run the testcase
    auto ec = orchestrator.run(tc);
    const bool pass = (tc.is_pass() &&
                       (ec == framework::error_code::NONE));
    // Output result
    std::cout << std::flush
              << "[Testing] "
              << (pass ? ("PASS " + tc.name) : "FAIL")
              << std::endl;

    return 0;
}

bool testcase::check_data(
    const mixnet_packet *const packet,
    const std::string& expected) const {

    auto rh = reinterpret_cast<const
        mixnet_packet_routing_header*>(packet->payload());

    const uint16_t rh_size = (
        sizeof(mixnet_packet_routing_header) +
        (sizeof(mixnet_address) * rh->route_length));

    const uint16_t expected_size = (sizeof(mixnet_packet) +
                                    rh_size + expected.size());

    // Packet size does not match expected value, fail early
    if (packet->total_size != expected_size) { return false; }

    auto data = (reinterpret_cast<const char*>(rh) + rh_size);
    return (memcmp(data, expected.c_str(), expected.size()) == 0);
}

bool testcase::check_route(
    const mixnet_packet_routing_header *const rh,
    const std::vector<mixnet_address>& expected) const {

    // Route length differs from expected value, fail early
    if (rh->route_length != expected.size()) { return false; }

    for (uint16_t idx = 0; idx < expected.size(); idx++) {
        if (rh->route()[idx] != expected[idx]) { return false; }
    }
    return true;
}

void testcase::await_convergence() const {
    std::this_thread::sleep_for(std::chrono::
        milliseconds(max_convergence_time_ms_));
}

void testcase::await_packet_propagation() const {
    std::this_thread::sleep_for(std::chrono::
        milliseconds(max_propagation_time_ms_));
}

} // namespace testing
