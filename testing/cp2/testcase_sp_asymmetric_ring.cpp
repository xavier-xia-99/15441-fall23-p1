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
 * asymmetric link costs in a ring topology.
 */
class testcase_sp_asymmetric_ring final : public testcase {
private:
    std::vector<uint64_t> received_;
    std::vector<mixnet_address> expected_0_{};
    std::vector<mixnet_address> expected_1_{65534, 0, 81, 21, 42};
    std::vector<std::string> data_{"Ooh (Give you up)", "Ooh-ooh (Give you up)"};

public:
    explicit testcase_sp_asymmetric_ring() :
        testcase("testcase_sp_asymmetric_ring"), received_(2, 0) {}

    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) override {

        if (packet->type == PACKET_TYPE_DATA) {
            auto rh = reinterpret_cast<const
                mixnet_packet_routing_header*>(packet->payload());

            int dst_node_id = graph_->get_node_id(rh->dst_address);
            int src_node_id = graph_->get_node_id(rh->src_address);
            pass_pcap_ &= (((src_node_id == 0) && (dst_node_id == 1)) ||
                           ((src_node_id == 1) && (dst_node_id == 0)));

            pass_pcap_ &= (fragment_id == dst_node_id);
            if (pass_pcap_) { received_[src_node_id]++; }

            pass_pcap_ &= (
                (src_node_id == 0) ? check_route(rh, expected_0_) :
                                     check_route(rh, expected_1_));

            pass_pcap_ &= check_data(packet, data_[src_node_id]);
            pcap_count_++;
        }
        // Unexpected packet type
        else { pass_pcap_ = false; }
    }

    virtual void setup() override {
        init_graph(7);
        graph_->set_mixaddrs({15, 31, 65534, 0, 81, 21, 42});
        graph_->generate_topology(graph::type::LINE, {1, 2, 3, 4, 5, 6, 0});
        graph_->add_edge(graph::half_edge(0, 5), graph::half_edge(1, 12345));
    }

    virtual error_code run(orchestrator& o) override {
        await_convergence(); // Await STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Try two nodes as source
        for (size_t idx = 0; idx < 21; idx++){
            DIE_ON_ERROR(o.send_packet(0, 1, PACKET_TYPE_DATA, data_[0]));
            DIE_ON_ERROR(o.send_packet(1, 0, PACKET_TYPE_DATA, data_[1]));
        }

        await_packet_propagation();
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = ((received_[0] == 21) &&
                          (received_[1] == 21) &&
                          (pcap_count_ == 42));
    }
};

int main(int argc, char **argv) {
    testcase_sp_asymmetric_ring tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
