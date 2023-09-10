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
#include "node.h"

#include "connection.h"
#include "packet.h"
#include "packet_init.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct node {
  uint8_t num_neighbors;  // number of neighbors
  int16_t *neighbors_addy; // Array of neighbors
  bool *neighbors_blocked; // Block of neighbors

  mixnet_address root_addr; // root addr
  mixnet_address my_addr; // self addr
  mixnet_address next_hop; // Next hop
  int path_len;

};


// Declare functions
void receive_and_update(void *const handle, struct node *node);
void sendSTPs(void *const handle, struct node *node);

/**
 * @brief This function is called send STPS constatnly
 * 
 * @param handle 
 * @param node 
 */
void sendSTPs(void *const handle, struct node *node) {
    //initialize a packet and send to all neighbors
    //don't have to bother w blocks, stp should send everywhere
    for (int i = 0; i < *node->neighbors_addy; i++) {
            mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node -> my_addr);
            mixnet_send(handle, i, discover_packet); //TODO error_handling
    }
}


/**
 * @brief This function is entrance point into each node
 * 
 * @param handle 
 * @param keep_running 
 * @param c 
 */
void run_node(void *const handle,
              volatile bool *const keep_running,
              const struct mixnet_node_config config) {

    (void) config;
    (void) handle;

    // Initialize Node
    struct node* node = malloc(sizeof(struct node));
    node->num_neighbors = config.num_neighbors;
    node->neighbors_addy = malloc(sizeof(int) * config.num_neighbors);
    node->neighbors_blocked = malloc(sizeof(bool) * config.num_neighbors);
    if (node->neighbors_addy == NULL || node->neighbors_blocked == NULL) {
        exit(1);
    }
    for (int i = 0; i < config.num_neighbors; i++) {
        node->neighbors_addy[i] = -1;
    }
    for (int i = 0; i < config.num_neighbors; i++) {
        node->neighbors_blocked[i] = false;
    }

    node->root_addr = config.node_addr; // I AM THE ROOT! -> this is teh real root
    node->my_addr = config.node_addr;
    node->next_hop = config.node_addr; // self
    node->path_len = 0;



    //discovering neighbor logic -> sends out messages to all neighbors
    //receiving and updating neighbor states
    sendSTPs(handle, node);
    receive_and_update(handle, node);

    clock_t start_time = clock();
    
    while(*keep_running) {
        // i'm root, and its time to send a root hello
        if (node->root_addr == node->my_addr && clock() - start_time >= config.root_hello_interval_ms) {
            sendSTPs(handle, node);
            start_time = clock();
        }

        // hey, i'm not the root, but i haven't got a root hello in a while. i'm root now!
        else if (node->root_addr != node->my_addr && clock() - start_time >= config.reelection_interval_ms) {
            node->root_addr = node->my_addr;
            node->path_len = 0;
            node->next_hop = node->my_addr;
            sendSTPs(handle, node);
            start_time = clock();
        }

        // if i'm not a root, listen for root_hello first, then broadcast after
        else if (node->root_addr != node->my_addr) {
            receive_and_update(handle, node);
            start_time = clock();
        }
        // nothing triggered, just continue till time reaches
    }
}


/**
 * @brief // Given a node, set the neighbors list of the neighbors interms of uint8_t
 * 
 * @param handle 
 * @param currNode 
 */
void receive_and_update(void *const handle, struct node *currNode) {
    (void) handle;

    // i received, and my neighbors is -1, so update address
    // i received, and i am not the root node, so i will send STP out again

    for (uint8_t i = 0; i < currNode->num_neighbors; i++) {

        mixnet_packet* stp_packet = initialize_STP_packet(
            currNode->root_addr, currNode->path_len, currNode->my_addr); // Free-d

        bool recv = mixnet_recv(handle, &i, &stp_packet);

        if (recv){
            mixnet_packet_stp *update = (mixnet_packet_stp*)malloc(sizeof(mixnet_packet_stp)); // Free-d
            memcpy((void *)update, (void *)stp_packet->payload, sizeof(mixnet_packet_stp));
                
            // Update if the neighbors list are NULL
            if (currNode->neighbors_addy[i] == -1) {
              currNode->neighbors_addy[i] = update->node_address;
              currNode->neighbors_blocked[i] = false;
            }
            else {
                bool updated = false;

                // Received lower_root_address
                if (update->root_address < currNode->root_addr) {
                    currNode->root_addr = update->root_address;
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    updated = true;
                } 
                // Received lower_path_length
                else if (update->root_address == currNode->root_addr &&
                    update->path_length + 1 < currNode->path_len) {
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    updated = true;
                } 
                // Received lower_address for next_hop
                else if (update->root_address == currNode->root_addr &&
                    update->path_length + 1 == currNode->path_len &&
                    update->node_address < currNode->next_hop) {
                    currNode->next_hop = update->node_address;
                    updated = true;
                }

                // If this update_packet was not useful, block the port
                if (!updated) {
                    currNode->neighbors_blocked[i] = true;
                }
            }
            free(update);
        }
        // if (!recv){
        //     currNode->neighbors_blocked[i] = true;
        // }

        free(stp_packet->payload);
        free(stp_packet);
    }
}
