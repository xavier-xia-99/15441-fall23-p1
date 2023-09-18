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
 * Exercises shortest-path routing of data packets with uniform,
 * symmetric link costs in a mesh topology. Also, checks whether
 * ties are broken by lowest mixnet address of the next hop.
 */
class testcase_sp_uniform_mesh final : public testcase {
private:
    std::vector<mixnet_address> expected_route_{15, 14};
    std::string data_{"Never gonna run around and desert you"};

public:
    explicit testcase_sp_uniform_mesh() :
        testcase("testcase_sp_uniform_mesh") {}

    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) override {

        if (packet->type == PACKET_TYPE_DATA) {
            auto rh = reinterpret_cast<const
                mixnet_packet_routing_header*>(packet->payload());

            pass_pcap_ &= (fragment_id == 6);
            pass_pcap_ &= (rh->dst_address ==
                           graph_->get_node(fragment_id).mixaddr());

            int src_node_id = graph_->get_node_id(rh->src_address);
            pass_pcap_ &= (src_node_id == 0);

            pass_pcap_ &= check_route(rh, expected_route_);
            pass_pcap_ &= check_data(packet, data_);
            pcap_count_++;
        }
        // Unexpected packet type
        else { pass_pcap_ = false; }
    }

    virtual void setup() override {
        init_graph(7);
        graph_->set_mixaddrs({13, 14, 15, 42, 4, 65534, 98});

        graph_->generate_topology(graph::type::STAR, {2, 0, 1, 3, 4});
        graph_->generate_topology(graph::type::STAR, {5, 2, 3, 4, 6});
        // Form a few more loops
        graph_->add_edge(1, 6);
        graph_->add_edge(3, 6);
    }

    virtual error_code run(orchestrator& o) override {
        await_convergence(); // Await STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Send a few packets
        for (size_t idx = 0; idx < 10; idx++) {
            DIE_ON_ERROR(o.send_packet(0, 6, PACKET_TYPE_DATA, data_));
        }
        await_packet_propagation();
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == 10);
    }
};

int main(int argc, char **argv) {
    testcase_sp_uniform_mesh tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
