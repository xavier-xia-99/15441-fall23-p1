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
#include "common/testing.h"

/**
 * Exercises random routing in a ring topology.
 */
class testcase_random final : public testcase {
private:
    bool saw_distinct_routes_ = false;
    std::vector<mixnet_address> first_route_;
    std::string data_{"Never gonna give you up"};

public:
    explicit testcase_random() :
        testcase("testcase_random") {}

    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) override {

        if (packet->type == PACKET_TYPE_DATA) {
            auto rh = reinterpret_cast<const
                mixnet_packet_routing_header*>(packet->payload());

            pass_pcap_ &= (fragment_id == 1);
            pass_pcap_ &= (rh->src_address == 15);
            pass_pcap_ &= (rh->dst_address == 31);
            pass_pcap_ &= check_data(packet, data_);
            pass_pcap_ &= (rh->route_length <= MAX_MIXNET_ROUTE_LENGTH);

            std::vector<mixnet_address> current_route;
            for (uint16_t idx = 0; (idx < rh->route_length)
                                    && pass_pcap_; idx++) {
                current_route.push_back(rh->route()[idx]);
            }
            if (pcap_count_ == 0) {
                first_route_ = current_route;
            }
            else {
                // Saw two or more distinct routes, seems random :)
                saw_distinct_routes_ |= (first_route_ != current_route);
            }
            pcap_count_++;
        }
        // Unexpected packet type
        else { pass_pcap_ = false; }
    }

    virtual void setup() override {
        init_graph(7);
        graph_->generate_topology(graph::type::RING);
        graph_->set_mixaddrs({15, 31, 65534, 0, 81, 21, 42});

        // Enable random routing for the source node
        graph_->get_node(0).set_use_random_routing(true);
    }

    virtual error_code run(orchestrator& o) override {
        await_convergence(); // Await STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Send many packets
        for (size_t idx = 0; idx < 100; idx++) {
            DIE_ON_ERROR(o.send_packet(0, 1, PACKET_TYPE_DATA, data_));
        }
        await_packet_propagation();
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == 100);
        pass_teardown_ &= saw_distinct_routes_;
    }
};

int main(int argc, char **argv) {
    testcase_random tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
