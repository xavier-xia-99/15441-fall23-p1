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
#include "utils.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INF 0xFFFF


//structure of our Node 
struct Node {
  uint8_t num_neighbors;  // number of neighbors
  mixnet_address *neighbors_addy; // Array of neighbors (mixnet address)
  uint16_t *neighbors_cost;
  bool *neighbors_blocked; // Block of neighbors (false is unblocked)
  
  int16_t total_neighbors_num;

  mixnet_address* global_best_path[1 << 16][1 << 8]; // List of [Path := List of mixnet_address
  mixnet_lsa_link_params* graph[1 << 16][1 << 8]; // 2^16 nodes, 2^8 List : []]
 
  mixnet_address root_addr; // root addr
  mixnet_address my_addr; // self addr
  mixnet_address next_hop; // Next hop
  int path_len;


    // graph[i][j] -> 
//   mixnet_address **FIB // List of [Path := List of mixnet_address]

};


//TODO: REMOVE 
struct RoutingHeader {
    mixnet_address src_addr; // given by user of mixnet_node
    mixnet_address dest_addr; // given by user of mixnet_node
    uint16_t route_len; // len(route)
    uint16_t route_index; // pointer at route (using pointer arithmetic)
};


// Declare functions

void print_bestpaths(struct Node* node);
void print_graph(struct Node* node);
void print_node(struct Node* node, bool verbose);
void receive_and_update(void *const handle, struct Node *node);
// void send_packet(void *const handle, struct node *node, enum mixnet_packet_type_enum type, uint16_t sender_port);
bool receive_STP(struct Node *currNode, uint8_t i, mixnet_packet *stp_packet, void *const handle);
void receive_and_send_LSA(mixnet_packet* LSA_packet, void* handle , struct Node * node, uint16_t sender_port);
void dijkstra(struct Node * node);
void construct_shortest_path(mixnet_address toNodeAddress, struct Node* node, mixnet_address *prev_neighbor);
  /**
   * @brief This function is called send STP constantly
   *     initialize a packet and send to all neighbors
   *     don't have to bother w blocks, stp should send everywhere
   *     node->num_neighbors is the port for the user itself
   *
   * @param handle
   * @param node
   */
void send_STP(void *const handle, struct Node *node) {
    for (int i = 0; i < node->num_neighbors; i++) {
        // printf("Sending out STP to %d \n", node->neighbors_addy[i]);
        mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node->my_addr);
        // print_packet(discover_packet);
        mixnet_send(handle, i, discover_packet); 
    }
}


  /**
   * @brief This function sends Flood Packet, and takes note of the sender port. 
   *
   * @param handle
   * @param node
   * @param sender_port (do not send back)
   */
void send_FLOOD(void *const handle, struct Node *node, uint16_t sender_port) {
        // printf("------Node #%u 's NB_INDX:%d sent FLOOD------------\n", node->my_addr, sender_port);
        for (int i = 0; i < node->num_neighbors; i++) {
                if (!node->neighbors_blocked[i] && i != sender_port){
                    // printf("Sending out FLOOD to NB_INDX:%u | addr:%u \n", i, (unsigned int)node->neighbors_addy[i]);
                    mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
                    mixnet_send(handle, i, flood_packet); //TODO error_handling
                }
        }

        // Always send to my user, unless I received it from the user!
        if (sender_port != node->num_neighbors){
            mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
            mixnet_send(handle, node->num_neighbors, flood_packet);
        }
    }


uint32_t get_time_in_ms(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000) + (time.tv_nsec / 1000000);
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

    // Initialize Node from the handle
    struct Node* node = malloc(sizeof(struct Node));
    node->num_neighbors = config.num_neighbors;
    node->total_neighbors_num = 0;
    node->neighbors_addy = malloc(sizeof(mixnet_address) * config.num_neighbors);
    node->neighbors_blocked = malloc(sizeof(bool) * config.num_neighbors);
    node->neighbors_cost = config.link_costs;
    for (int i = 0; i < node->num_neighbors; i ++){
        printf(" idx:%d , cost  :%d \n", i, node->neighbors_cost[i]);
    }

    if (node->neighbors_addy == NULL || node->neighbors_blocked == NULL) {
        exit(1);
    }
    for (int i = 0; i < config.num_neighbors; i++) {
        node->neighbors_addy[i] = INVALID_MIXADDR;
    }
    
    for (int i = 0; i < config.num_neighbors; i++) {
        node->neighbors_blocked[i] = true; // initialize all to un-blocked
    }

    node->root_addr = config.node_addr; // I AM THE ROOT! 
    node->my_addr = config.node_addr;
    node->next_hop = config.node_addr; // self
    node->path_len = 0;


    // printf("Node initialized! \n");

    send_STP(handle, node); // SEND STP 
    // printf("Set up neighbours");
    // print_node(node);
    
    uint32_t re_election_time = get_time_in_ms();
    uint32_t heartbeat_time = get_time_in_ms();
    uint32_t lsa_time = get_time_in_ms();
    // printf("Starting time: %d \n", start_time);

    //allocate chunk of memory for buffer
    mixnet_packet *packet_buffer =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));

    // printf("Packet memory initialized\n");

    if (packet_buffer == NULL) {
        exit(1);
    }
    uint16_t num_user_packets = 0;
    // uint8_t node_id = node->my_addr; 

    while(*keep_running) {
        //check if keep running is true
        if (!(*keep_running)) {
            printf("Keep running not true anymore. break!");
            break;
        }

        uint8_t port;
        bool recv = mixnet_recv(handle, &port, &packet_buffer);

        // receiving from all my neighbors
        // uint32_t curr_time = get_time_in_ms();

        // i'm root, and its time to send a root hello
        if (node->root_addr == node->my_addr && get_time_in_ms() - heartbeat_time >= config.root_hello_interval_ms) {
            send_STP(handle, node);
            heartbeat_time = get_time_in_ms();
        }

        //SENDING OUT LSA's every 10 root hellos
        if (get_time_in_ms() - lsa_time >= config.root_hello_interval_ms*10) {
            //packs my current state into my graph
            for (int i = 0; i < config.num_neighbors; i++) {
                // make this struct to stuff in
                // CAST (mixnet_lsa_link_params*)
                    if (node->graph[node->my_addr][i] == NULL) {
                        node->graph[node->my_addr][i] = (mixnet_lsa_link_params*) malloc(sizeof(mixnet_lsa_link_params));
                        node->graph[node->my_addr][i]->neighbor_mixaddr = node->neighbors_addy[i];
                        node->graph[node->my_addr][i]->cost = node->neighbors_cost[i];
                    }
            }

            // Send out my own LSA_packet at the start of heartbeat
            // printf("Initializing LSA \n");
            mixnet_packet* LSA_packet = initialize_LSA_packet(node->my_addr, node->num_neighbors,node->neighbors_addy, node->neighbors_cost);
            // printf("LSA Packet done! \n");
            for (int i = 0; i < node->num_neighbors; i++) {
                if (!node->neighbors_blocked[i]){
                    // printf("Sending out FLOOD to NB_INDX:%u | addr:%u \n", i, (unsigned int)node->neighbors_addy[i]);
                    if (!mixnet_send(handle, i, LSA_packet)){
                        exit(1);
                    }; //TODO error_handling
                }

            }
            printf("Initialize State + sent out LSA from %u \n", node->my_addr);
            lsa_time = get_time_in_ms();
        }

        // hey, i'm not the root, but i haven't got a root hello in a while. i'm root now!
        if (node->root_addr != node->my_addr && get_time_in_ms() - re_election_time >= config.reelection_interval_ms) {
            node->root_addr = node->my_addr;
            node->path_len = 0;
            node->next_hop = node->my_addr;
            send_STP(handle, node);
            re_election_time = get_time_in_ms();
        }
    
        // we received something. 
        if (recv) {
            // printf("Node: %d received packet! @ Port : %d \n", node_id, port);
            // printf("Packet Type : %s \n", get_packet_type(packet_buffer));
            // printf("RECEIVED Packet! \n");

            if (packet_buffer->type == PACKET_TYPE_STP || packet_buffer->type == PACKET_TYPE_FLOOD) {
                // Copying Payload over for STP/Flood Types
                mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
                memcpy((void *)update, (void *)packet_buffer->payload,  sizeof(mixnet_packet_stp));

                if (packet_buffer->type == PACKET_TYPE_STP){
                    // printf("received STP packet");
                    if (update->root_address == node->root_addr && update->path_length + 1 == node->path_len && update->node_address == node->next_hop) {
                        // mixnet_packet* new_packet = initialize_STP_packet(node->root_addr,node->path_len,node->my_addr);
                        // mixnet_send(handle, port, new_packet);
                        send_STP(handle, node);
                        re_election_time = get_time_in_ms(); 
                        } 
                    else {
                        receive_STP(node, port, packet_buffer, handle);
                    }
                }

                else if (packet_buffer->type == PACKET_TYPE_FLOOD){
                    if (port == node->num_neighbors) {
                        num_user_packets++;
                        printf("Received FLOOD from user! \n");
                        // initializes graph's for my own thing
                        
                        send_FLOOD(handle, node, port);
                        } 
                    else if (!node->neighbors_blocked[port]){
                        send_FLOOD(handle, node, port);
                        }
                }
            }
            // LSA Packets Handling
            else if (packet_buffer->type == PACKET_TYPE_LSA){
                printf("LSA Packet!\n");
                // If received from unblocked ports and not from user   
                if (!node->neighbors_blocked[port] && port != node->num_neighbors){
                    receive_and_send_LSA(packet_buffer, handle, node, port);
                }
                
                print_node(node, true);
            }

            // TODO : Packets that uses Routing Headers
            else {
                assert(packet_buffer->type == PACKET_TYPE_DATA || packet_buffer->type == PACKET_TYPE_PING);
                // print_graph(node);
                uint32_t start = get_time_in_ms();
                dijkstra(node);
                printf("Dijkstra's took %u ms \n", get_time_in_ms() - start);
                print_bestpaths(node);
                //node->best path is the best path to any node from my current node.
                if (packet_buffer->type == PACKET_TYPE_DATA){

                    //if user sent packet, route length will be 0, need to compute route adn intialize routing header + copy packet data after
                    if (port == node->num_neighbors){

                        // // Loop through each destination
                        // for (int i = 0; i < (1 << 16); ++i) {
                        //     // Loop through each path for a given destination
                        //     for (int j = 0; j < (1 << 8); ++j) {
                        //         mixnet_address* current_path = node->global_best_path[i][j];
                        //         // Make sure the path is not NULL before dereferencing
                        //         if (current_path != NULL) {
                        //             // printf("Destination: %d\n", i);
                        //             // printf("Path[%d][%d]: %u\n", i, j, *current_path);
                        //         }
                        //     }
                        // }

                        // uint16_t totalsize = packet_buffer->total_size;
                        
    //                   Finally, for DATA packets, the
    //  *                data will appear after the Routing Header (RH) assuming
    //  *                zero hops (i.e., at the `route` field of mixnet_packet_
    //  *                routing_header). You must fix-up the payload structure
    //  *                after you compute the source route and hop count.

                        //----------------------parsing-------------------

                        //this should be pointer to payload
                        mixnet_address src_address, dest_address;
                        memcpy(&src_address, packet_buffer->payload, sizeof(mixnet_address));
                        memcpy(&dest_address, packet_buffer->payload + sizeof(mixnet_address), sizeof(mixnet_address));
                        char* data = (char*) malloc(MAX_MIXNET_DATA_SIZE);
                        
                        //can i just cpy max_mixnet_data size over? (size == 8?)
                        memcpy(data, packet_buffer->payload + 2 * sizeof(mixnet_address) + 2 * sizeof(uint16_t), MAX_MIXNET_DATA_SIZE);

                        mixnet_address** best_paths = node->global_best_path[dest_address];
                        
                        printf("this is what destination we are trying to send to %u \n", dest_address);
                        
                        mixnet_packet* final_data_packet = initialize_DATA_packet(best_paths, dest_address, src_address, data);
                        //find hope index and route length
                        mixnet_packet_routing_header* routing_header = (mixnet_packet_routing_header*) (final_data_packet->payload);
                        uint16_t hop_idx = routing_header->hop_index;

                        //can i just do this?
                        mixnet_address nexthop = routing_header->route[hop_idx];
                        uint16_t nextport = 0;
                        for (int i = 0; i < node->num_neighbors; i++){
                            if (node->neighbors_addy[i] == nexthop){
                                nextport = i;
                                break;
                            }
                        }
                        mixnet_send(handle, nextport, final_data_packet);
                    } else {
                        //this is receiving from other nodes (NOT USER PORT)
                        //need to add 1 to hop index
                        //and getpacket_>buffer

                        mixnet_packet_routing_header* routing_header = (mixnet_packet_routing_header*) (packet_buffer->payload);
                        uint16_t hop_idx = routing_header->hop_index;
                        uint16_t route_length = routing_header->route_length;

                        //i've reached -> am done, thanks. send to my own user
                        if (node->my_addr == routing_header->dst_address){
                            assert(hop_idx == routing_header->route_length - 1);
                            mixnet_send(handle, node->num_neighbors, packet_buffer);
                            break;
                        }
                        // Right before destination
                        else if (hop_idx + 1 == route_length){
                            mixnet_send(handle, routing_header->dst_address, packet_buffer);
                            break;
                        }
                        else {
                            //else, send to next one in the route_length
                            hop_idx ++;
                            routing_header->hop_index = hop_idx;
                            mixnet_address nexthop = routing_header->route[hop_idx];
                            uint16_t nextport = 0;
                            for (int i = 0; i < node->num_neighbors; i++){
                                if (node->neighbors_addy[i] == nexthop){
                                    nextport = i;
                                    break;
                                }
                            }
                            mixnet_send(handle, nextport, packet_buffer);
                        }
                    }
                }

                else if (packet_buffer->type == PACKET_TYPE_PING){

                    // if (port == node->num_neighbors){
                    //     //set up my own ping packet
                    //     mixnet_address src_address, dest_address;
                    //     memcpy(&src_address, packet_buffer->payload, sizeof(mixnet_address));
                    //     memcpy(&dest_address, packet_buffer->payload + sizeof(mixnet_address), sizeof(mixnet_address));
                    //     char* data = (char*) malloc(MAX_MIXNET_DATA_SIZE);
                        
                    //     //can i just cpy max_mixnet_data size over?
                    //     memcpy(data, packet_buffer->payload + 2 * sizeof(mixnet_address) + 2 * sizeof(uint16_t), MAX_MIXNET_DATA_SIZE);

                    //     mixnet_address** best_paths = node->global_best_path[dest_address];
                        
                    //     mixnet_packet* final_ping_packet = initialize_PING_packet(best_paths, dest_address, src_address);


                    // }

                    // else {

                    //     //rmbr check destination and then send back also

                    // }


                // }

                printf("Received Data/Ping Packet! \n");
                }

            }
        }
    }
    printf("================FINAL NODE=================");
    print_graph(node);
    printf("\n Node[#%d] Stats: \n | Received %d user packets | Node[#%d] stopped running \n", node->my_addr, num_user_packets, node->my_addr);
    // print_node(node);
}


void dijkstra(struct Node * node){
    // printf("Running Dijkstra's Algorithm! \n");
    bool visited[1<<16];
    uint16_t distance[1<<16];
    mixnet_address prev_neighbor[1<<16];

    for (int i = 0; i < (1<<16); i++) {
        visited[i] = false;
        distance[i] = UINT16_MAX;
        prev_neighbor[i] = -1; //? question mark? 
    }
    //run through the old graph, for every neighbor node that's not myself

    int visitedCount = 0;
    distance[node->my_addr] = 0; // distance to self == 0
    visited[node->my_addr] = false; // visited self
    mixnet_address smallestindex = node->my_addr;

    while (visitedCount < node->total_neighbors_num + 1) {
        //TODO: find min value from everything in index and visit it 
        //(node w least distance), visit that shit
        uint16_t min_number = UINT16_MAX;
        for (int i = 0; i < (1<<16); i ++){
            if (!visited[i] && distance[i] < min_number){
                smallestindex = i;
                min_number = distance[i];
            }
        }
        assert(min_number != UINT16_MAX);
        printf("--------------------------------------\n");
        printf("we're visiting this node %u\n", smallestindex);

        //TODO: explore all edges of curr node and check, update prev neighbor if updated
        for (int i = 0; i < (1<<8); i ++){
            //means there's edge
            if (node->graph[smallestindex][i] != NULL){

                mixnet_address edge_address = node->graph[smallestindex][i]->neighbor_mixaddr;
                uint16_t edge_cost = node->graph[smallestindex][i]->cost;

                printf("we're visiting neighbor %u with cost %u\n", edge_address, edge_cost);

                //doesn't matter if i've been there before
                uint16_t final_cost = 0;
                final_cost = edge_cost + distance[smallestindex];
                if (final_cost < distance[edge_address]){
                    distance[edge_address] = final_cost;
                    prev_neighbor[edge_address] = smallestindex;
                }
            }

            else {
                break;
            }
        }
        visited[smallestindex] = true;
        visitedCount ++;
    }
    printf("we've looped this many times %u\n", visitedCount);
    printf("this is actually how many neighbors we have %u\n", node->total_neighbors_num);

    //TODO: build shortest paths from our distance and state graph

      printf("distance and visited array: ");
        for (int i = 0; i < (1<<16); ++i) {
            if (distance[i] != UINT16_MAX) {
                char* message = visited[i] ? "True" : "False";
                printf("index is %u, distance is %u, did i visit this %s \n",i, distance[i],message);
            }
        }
        printf("\n");

    for (int i = 0; i < (1<<16); i ++){
        if (distance[i] != UINT16_MAX && i != node->my_addr){
            printf("this is where I'm going %u \n", i);
            assert(visited[i]);
            construct_shortest_path(i, node, prev_neighbor);
        }
    }
}

//trace back the visited path and add to global best path
void construct_shortest_path(mixnet_address toNodeAddress, struct Node* node, mixnet_address *prev_neighbor){
    mixnet_address curr_node = toNodeAddress;
    int counter = 0;
    while (curr_node != node->my_addr){
        node->global_best_path[toNodeAddress][counter] = (mixnet_address*) malloc(sizeof(mixnet_address));
        *node->global_best_path[toNodeAddress][counter] = prev_neighbor[curr_node];
        curr_node = prev_neighbor[curr_node];
        counter ++;
    }
    assert(curr_node == node->my_addr);
  
}
    

// @TODO : Receives LSA : (1) Update Link State, (2) Sends Out to Unblocked Ports
void receive_and_send_LSA(mixnet_packet* LSA_packet, void* handle , struct Node * node, uint16_t sender_port){
    assert(!node->neighbors_blocked[sender_port] && sender_port != node->num_neighbors);
    assert(LSA_packet->type == PACKET_TYPE_LSA);

    // (1) Update Link State using nb_link_params
    mixnet_packet_lsa *lsa_header = (mixnet_packet_lsa *)malloc(sizeof(mixnet_packet_lsa));
    if (lsa_header == NULL) {
        printf("no space");
        exit(1);
    }
    memcpy((void *)lsa_header, (void *)LSA_packet->payload,  sizeof(mixnet_packet_lsa));

    // [2] Allocate Space dynamically for the #nbs * link_params

    // Copy all nb's link_params over
    assert(lsa_header->neighbor_count > 0);
    unsigned long nb_size = sizeof(mixnet_lsa_link_params) * lsa_header->neighbor_count;
    
    mixnet_lsa_link_params* nb_link_params = (mixnet_lsa_link_params *)malloc(nb_size);
    if (nb_link_params == NULL) {
        printf("no space");
        exit(1);
    }

    memcpy((void *)nb_link_params, (void *)(LSA_packet->payload + sizeof(mixnet_address) + sizeof(uint16_t)),  nb_size);
    
    // Update Graph based on my state
    for (int i = 0; i < lsa_header->neighbor_count; i++) {
        // make this struct to stuff in
        mixnet_address node_addr = lsa_header->node_address;
        mixnet_address neighbor_addr = nb_link_params[i].neighbor_mixaddr;
        uint16_t cost = nb_link_params[i].cost;
        assert(cost > 0);


        if (node->graph[node_addr][0] == NULL){
            node->total_neighbors_num++;
        }

        // CAST (mixnet_lsa_link_params*)
        if (node->graph[node_addr][i] == NULL) {
            node->graph[node_addr][i] = (mixnet_lsa_link_params*) malloc(sizeof(mixnet_lsa_link_params));
            node->graph[node_addr][i]->neighbor_mixaddr = neighbor_addr;
            node->graph[node_addr][i]->cost = cost;
            //run djikstra's here. point is for our graph, we want to find the shortest path to any node_addr
            //node->global_best_path should contain best path to everything.
        }
    }

    // (2) Broadcast Received LSA_packet to Unblocked Ports Only, excluding sender_port
    for (int i = 0; i < node->num_neighbors; i++) {
        if (!node->neighbors_blocked[i] && i != sender_port){
            // printf("Sending out FLOOD to NB_INDX:%u | addr:%u \n", i, (unsigned int)node->neighbors_addy[i]);
            if (!mixnet_send(handle, i, LSA_packet)){
                exit(1);
            } //TODO error_handling
        }
    }
    
}

// Does not reach this function at all
bool receive_STP(struct Node * currNode, uint8_t port, mixnet_packet* stp_packet, void *const handle){
    // printf("\n[Received STP packet!]\n");
    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); // Free-d
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));

    
    if (currNode->neighbors_addy[port] == INVALID_MIXADDR) {
        currNode->neighbors_addy[port] = update->node_address;
        currNode->neighbors_blocked[port] = false; // unblock
        return true;
    } else {
        bool state_changed = false;
        // Received lower_root_address
        if (update->root_address < currNode->root_addr) {
                currNode->root_addr = update->root_address;
                currNode->path_len = update->path_length + 1;
                currNode->next_hop = update->node_address;
                state_changed = true;
                // old_root = currNode->root_addr;
        }
        // Received lower_path_length
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 < currNode->path_len) {
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    state_changed = true;
        }
        // Received lower_address for next_hop
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 == currNode->path_len &&
                 update->node_address < currNode->next_hop) {
                    currNode->next_hop = update->node_address;
                    state_changed = true;
        }

        if (state_changed){
            // Reset All Nodes
            send_STP(handle, currNode);

            // Need this to block all neighbors
            for (int i = 0; i < currNode->num_neighbors; i++) {
                currNode->neighbors_blocked[i] = true;
            }

        }
    // ======================= BLOCK LOGIC =======================
        // Parent is unblocked
        if (update->node_address == currNode->next_hop){
            currNode->neighbors_blocked[port] = false; 
        }

        // Unblock potential children 
        else if (update->root_address == currNode->root_addr &&
                update->path_length == currNode->path_len + 1){
                // printf("Node #%d, blocking my potential Parent, who is strictly worse \n", currNode->my_addr);
                currNode->neighbors_blocked[port] = false;
        }
        else {
            currNode->neighbors_blocked[port] = true;
        }
    // ======================= BLOCK LOGIC =======================
        
    }
    return true;
}


void print_node(struct Node *node, bool verbose) {
    printf("-----------------Printing Node [#%d]!------------------- \n", node->my_addr);
    printf("Root Addr: %d \n", node->root_addr);
    printf("Path Len: %d \n", node->path_len);
    printf("Node Addr: %d \n", node->my_addr);
    printf("Next Hop: %d \n", node->next_hop);
    printf("Num Neighbors %d \n", node->num_neighbors);
    printf("Total Nodes in Graph %d \n", node->total_neighbors_num);
    printf("Neighbors List : \n");
    for (int i = 0; i < node->num_neighbors; i++) {
        const char* value = (node->neighbors_blocked[i]) ? "Yes" : "No";
        printf("Neighbor Index #%d | Address: %d | Blocked: %s | \n", i, node->neighbors_addy[i], value);
    }
    if (verbose) print_graph(node);
    printf("--------------Printing Node [#%d] Complete!--------------\n",node->my_addr);
}
// Prints out graph of node
void print_graph(struct Node* node) {
    printf("_____________ [GRAPH] of Node [#%d]! _____________  \n", node->my_addr);
    for (int i = 0; i < node->num_neighbors; i++) {
        if (node->graph[node->my_addr][i] != NULL) {  // Check if the pointer is valid
            printf("Neighbor Index #%d | (Address: %d , Cost: %d ) \n", 
                   i, 
                   node->graph[node->my_addr][i]->neighbor_mixaddr,  // Dereferencing pointer
                   node->graph[node->my_addr][i]->cost);  // Dereferencing pointe
        } else {
            printf("Neighbor Index #%d is not allocated.\n", i);
        }
    }
    printf("_____________  [GRAPH] of Node [#%d] Complete! _____________ \n", node->my_addr);
}

void print_bestpaths(struct Node* node) {
    printf("============ BESTPATHS of Src Node [#%d]! ============== \n", node->my_addr);
    for (int i = 0; i < (1<<16); i++) {
        if (node->global_best_path[i] != NULL){
            // Loop through each path for a given destination
            for (int j = 0; j < (1<<8); j++) {
                // Mixnet Addres of next_hop 
                if (node->global_best_path[i][j] != NULL) {
                    printf("KEY : %d\n", i);
                    // printf("-------------------\n");
                }
                else {
                    break;
                }
                mixnet_address* current_path = node->global_best_path[i][j];
                // Make sure the path is not NULL before dereferencing
                if (current_path != NULL) {
                    printf("Hop Index [%d]: Towards %u\n", j, *current_path);
                }
                else {
                    printf(" |END| \n");
                    assert(current_path == NULL);
                    break;
                }
            }
        }
        
        // printf("-------- [END] of Mixnet_addr:%d--------\n", i);
    }
    printf("============ BESTPATHS of Node [#%d] Complete! ==============\n", node->my_addr);
}


