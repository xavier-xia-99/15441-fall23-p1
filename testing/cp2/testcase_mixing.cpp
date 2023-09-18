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
 * Exercises mixing functionality in a star topology.
 * Checks whether the hub (with mixing factor 6) and
 * sources buffer packets properly.
 */
class testcase_mixing final : public testcase {
private:
    std::vector<uint64_t> received_;
    volatile bool expect_packets_ = false;
    std::string data_{"Never gonna let you down"};

public:
    explicit testcase_mixing() :
        testcase("testcase_mixing"), received_(7, 0) {}

    virtual void pcap(
        const uint16_t fragment_id,
        const mixnet_packet *const packet) override {

        pass_pcap_ &= expect_packets_;
        if (packet->type == PACKET_TYPE_DATA) {
            auto rh = reinterpret_cast<const
                mixnet_packet_routing_header*>(packet->payload());

            pass_pcap_ &= (fragment_id >= 4);
            pass_pcap_ &= (rh->dst_address ==
                           graph_->get_node(fragment_id).mixaddr());

            int src_node_id = graph_->get_node_id(rh->src_address);
            pass_pcap_ &= ((src_node_id != -1) && (src_node_id <= 3));

            pass_pcap_ &= (received_[fragment_id] < 4);
            if (pass_pcap_) { received_[fragment_id]++; }

            pass_pcap_ &= check_data(packet, data_);
            pcap_count_++;
        }
        // Unexpected packet type
        else { pass_pcap_ = false; }
    }

    virtual void setup() override {
        init_graph(7);
        graph_->set_mixaddrs({15, 31, 13, 64, 71, 21, 42});
        graph_->generate_topology(graph::type::STAR, {3, 0, 1, 2, 4, 5, 6});

        // Configure mixing factors
        graph_->get_node(3).set_mixing_factor(6);
        for (uint16_t i = 0; i < 3; i++) {
            graph_->get_node(i).set_mixing_factor(3);
        }
    }

    virtual error_code run(orchestrator& o) override {
        await_convergence(); // Await STP convergence

        // Subscribe to packets from all nodes
        for (uint16_t i = 0; i < graph_->num_nodes; i++) {
            DIE_ON_ERROR(o.pcap_change_subscription(i, true));
        }
        // Inject 3 packets at the hub
        for (uint16_t i = 0; i < 3; i++) {
            DIE_ON_ERROR(o.send_packet(3, (4 + i),
                         PACKET_TYPE_DATA, data_));
        }
        await_packet_propagation();

        // Inject 6 packets at the sources
        for (size_t idx = 0; idx < 2; idx++) {
            for (uint16_t i = 0; i < 3; i++) {
                DIE_ON_ERROR(o.send_packet(i, (4 + i),
                             PACKET_TYPE_DATA, data_));
            }
        }
        await_packet_propagation();

        // Inject the remaining packets
        expect_packets_ = true;
        for (uint16_t i = 0; i < 3; i++) {
            DIE_ON_ERROR(o.send_packet(i, (4 + i),
                         PACKET_TYPE_DATA, data_));
        }
        await_packet_propagation();
        return error_code::NONE;
    }

    virtual void teardown() override {
        pass_teardown_ = (pcap_count_ == 12);
    }
};

int main(int argc, char **argv) {
    testcase_mixing tc; // Run testcase
    return testcase::run_testcase(tc, argc, argv);
}
