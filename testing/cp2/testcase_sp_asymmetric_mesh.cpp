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
 * asymmetric link costs in a mesh topology.
 */
class testcase_sp_asymmetric_mesh final : public testcase {
private:
    std::vector<uint64_t> received_;
    std::vector<mixnet_address> expected_0_{98, 65534};
    std::vector<mixnet_address> expected_1_{15};
    std::vector<std::string> data_{"Never gonna give",
                                   "never gonna give (Give you up)"};

public:
    explicit testcase_sp_asymmetric_mesh() :
        testcase("testcase_sp_asymmetric_mesh"), received_(2, 0) {}

    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) override {

        if (packet->type == PACKET_TYPE_DATA) {
            auto rh = reinterpret_cast<const
                mixnet_packet_routing_header*>(packet->payload());

            int dst_node_id = graph_->get_node_id(rh->dst_address);
            int src_node_id = graph_->get_node_id(rh->src_address);
            pass_pcap_ &= (((src_node_id == 1) && (dst_node_id == 3)) ||
                           ((src_node_id == 3) && (dst_node_id == 1)));

            pass_pcap_ &= (fragment_id == dst_node_id);
            int normalized_src_node_id = (src_node_id - 1) / 2;
            if (pass_pcap_) { received_[normalized_src_node_id]++; }

            pass_pcap_ &= (
                (src_node_id == 1) ? check_route(rh, expected_0_) :
                                     check_route(rh, expected_1_));

            pass_pcap_ &= check_data(packet, data_[normalized_src_node_id]);
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
        graph_->add_edge(graph::half_edge(2, 2), graph::half_edge(3, 0));
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
        // Try two nodes as source
        for (size_t idx = 0; idx < 7; idx++){
            DIE_ON_ERROR(o.send_packet(1, 3, PACKET_TYPE_DATA, data_[0]));
            DIE_ON_ERROR(o.send_packet(3, 1, PACKET_TYPE_DATA, data_[1]));
        }

        await_packet_propagation();
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = ((received_[0] == 7) &&
                          (received_[1] == 7) &&
                          (pcap_count_ == 14));
    }
};

int main(int argc, char **argv) {
    testcase_sp_asymmetric_mesh tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
