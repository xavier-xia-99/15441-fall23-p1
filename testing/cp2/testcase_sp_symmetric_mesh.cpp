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
 * Exercises shortest-path routing of data packets with
 * non-uniform, symmetric link costs in a mesh topology.
 */
class testcase_sp_symmetric_mesh final : public testcase {
private:
    std::vector<mixnet_address> expected_route_{98, 65534};
    std::string data_{"Never gonna tell a lie and hurt you"};

public:
    explicit testcase_sp_symmetric_mesh() :
        testcase("testcase_sp_symmetric_mesh") {}

    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) override {

        if (packet->type == PACKET_TYPE_DATA) {
            auto rh = reinterpret_cast<const
                mixnet_packet_routing_header*>(packet->payload());

            pass_pcap_ &= (fragment_id == 3);
            pass_pcap_ &= (rh->dst_address ==
                           graph_->get_node(fragment_id).mixaddr());

            int src_node_id = graph_->get_node_id(rh->src_address);
            pass_pcap_ &= (src_node_id == 1);

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

        graph_->add_edge(4, 5);
        graph_->add_edge(3, 6);
        graph_->generate_topology(graph::type::LINE, {0, 2, 4});
        graph_->add_edge(graph::half_edge(1, 2), graph::half_edge(2, 2));
        graph_->add_edge(graph::half_edge(1, 3), graph::half_edge(6, 3));
        graph_->add_edge(graph::half_edge(2, 2), graph::half_edge(3, 2));
        graph_->add_edge(graph::half_edge(2, 2), graph::half_edge(5, 2));
        graph_->add_edge(graph::half_edge(5, 0), graph::half_edge(6, 0));
        graph_->add_edge(graph::half_edge(5, 0), graph::half_edge(3, 0));
    }

    virtual error_code run(orchestrator& o) override {
        await_convergence(); // Await STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Send a few packets
        for (size_t idx = 0; idx < 51; idx++) {
            DIE_ON_ERROR(o.send_packet(1, 3, PACKET_TYPE_DATA, data_));
        }
        await_packet_propagation();
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == 51);
    }
};

int main(int argc, char **argv) {
    testcase_sp_symmetric_mesh tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
