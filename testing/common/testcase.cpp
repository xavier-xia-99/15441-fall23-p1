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

#include <iostream>
#include <filesystem>
#include <memory>

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

} // namespace testing
