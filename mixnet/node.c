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
    uint16_t num_neighbors;  // number of neighbors
    mixnet_address *neighbors_addy; // Array of neighbors (mixnet address)
    uint16_t *neighbors_cost;
    bool *neighbors_blocked; // Block of neighbors (false is unblocked)

    uint16_t total_neighbors_num;

    mixnet_address* global_best_path[1 << 16][1 << 8]; // List of [Path := List of mixnet_address
    mixnet_lsa_link_params* graph[1 << 16][1 << 8]; // 2^16 nodes, 2^8 List : []]

    mixnet_address root_addr; // root addr
    mixnet_address my_addr; // self addr
    mixnet_address next_hop; // Next hop
    uint16_t path_len;

    bool visited[1<<16];
    uint16_t distance[1<<16];
    mixnet_address prev_neighbor[1<<16];
    uint16_t visitedCount;

};



// Declare functions
void print_routing_header(mixnet_packet_routing_header* header);
void print_bestpaths(struct Node* node);
void print_graph(struct Node* node);
void print_node(struct Node* node, bool verbose);
void print_globalgraph(struct Node* node);


void receive_and_update(void *const handle, struct Node *node);
// void send_packet(void *const handle, struct node *node, enum mixnet_packet_type_enum type, uint16_t sender_port);
bool receive_STP(struct Node *currNode, uint8_t i, mixnet_packet *stp_packet, void* handle);
void receive_and_send_LSA(mixnet_packet* LSA_packet, void* handle , struct Node * node, uint16_t sender_port);
void dijkstra(struct Node * node, bool verbose);
void construct_shortest_path(mixnet_address toNodeAddress, struct Node* node, mixnet_address *prev_neighbor);
uint16_t find_next_port(mixnet_packet_routing_header* routing_header, struct Node* node);  
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
    // bool verbose = false;
    // Initialize Node from the handle
    struct Node* node = malloc(sizeof(struct Node));
    node->num_neighbors = config.num_neighbors;
    node->total_neighbors_num = 0;
    node->neighbors_addy = malloc(sizeof(mixnet_address) * config.num_neighbors);
    node->neighbors_blocked = malloc(sizeof(bool) * config.num_neighbors);
    node->neighbors_cost = config.link_costs;
    node->visitedCount = 0;
    // for (int i = 0; i < node->num_neighbors; i ++){
    //     printf(" idx:%d , cost  :%d \n", i, node->neighbors_cost[i]);
    // }

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

    // printf("NODE :#%d initialized! \n", node->my_addr);
    


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
    uint16_t sent_to_users = 0;
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

        //SENDING OUT LSA's every _(?) root hellos
        if (get_time_in_ms() - lsa_time >= config.root_hello_interval_ms*15) {
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
                    mixnet_packet *temp = malloc(LSA_packet->total_size);
                    memcpy(temp, LSA_packet, LSA_packet->total_size);
                    // printf("Sending out FLOOD to NB_INDX:%u | addr:%u \n", i, (unsigned int)node->neighbors_addy[i]);
                    if (!mixnet_send(handle, i, temp)){
                        exit(1);
                    } //TODO error_handling
                }

            }
            lsa_time = get_time_in_ms();
        }

        // hey, i'm not the root, but i haven't got a root hello in a while. i'm root now!
        if (node->root_addr != node->my_addr && get_time_in_ms() - re_election_time >= config.reelection_interval_ms) {
            node->root_addr = node->my_addr;
            node->path_len = 0;
            node->next_hop = node->my_addr;
            send_STP(handle, node);
            for (uint8_t i = 0; i < node->num_neighbors; i++) {
                node->neighbors_blocked[i] = false;
            }
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
                    // parent, set everyone, receive every
                    if (update->root_address == node->root_addr && update->path_length + 1 == node->path_len && update->node_address == node->next_hop) {
                        // mixnet_packet* new_packet = initialize_STP_packet(node->root_addr,node->path_len,node->my_addr);
                        // mixnet_send(handle, port, new_packet);
                        for (int i = 0; i < node->num_neighbors; i++) {
                            if (i == port) continue;
                            // printf("Sending out STP to %d \n", node->neighbors_addy[i]);
                            mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node->my_addr);
                            // print_packet(discover_packet);
                            mixnet_send(handle, i, discover_packet); 
                        }
                        re_election_time = get_time_in_ms(); 
                    } 
                    else {
                        if (receive_STP(node, port, packet_buffer, handle)) {
                            re_election_time = get_time_in_ms();
                        }
                        // send_STP(handle, node);
                    }
                    // }
                }

                else if (packet_buffer->type == PACKET_TYPE_FLOOD){
                    
                    if (port == node->num_neighbors) {
                        num_user_packets++;
                        // printf("Received FLOOD from user! \n");
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
                // printf("LSA Packet!\n");
                // If received from unblocked ports and not from user   
                if (!node->neighbors_blocked[port] && port != node->num_neighbors){
                    receive_and_send_LSA(packet_buffer, handle, node, port);
                    // print_node(node, true);
                }
                
                // print_node(node, true);
            }
            // TODO : Packets that uses Routing Headers
            else {
                assert(packet_buffer->type == PACKET_TYPE_DATA || packet_buffer->type == PACKET_TYPE_PING);
                
                if (packet_buffer->type == PACKET_TYPE_DATA){
                    unsigned long total_size = packet_buffer->total_size;
                    mixnet_packet_routing_header* header = (mixnet_packet_routing_header* ) &packet_buffer->payload;
                    unsigned long data_size = total_size - sizeof(mixnet_packet) - sizeof(mixnet_packet_routing_header) - header->route_length * sizeof(mixnet_address);
                    
                    //if user sent packet, route length will be 0, need to compute route adn intialize routing header + copy packet data after
                    
                    if (port == node->num_neighbors){
                        // print_globalgraph(node);
                        num_user_packets++;
                        // print_globalgraph(node);
                        // print_node(node, true);

                        dijkstra(node, false);
                        char* data = (char*) malloc(data_size);
                        //can i just cpy max_mixnet_data size over? (size == 8?)
                        memcpy(data, (char*)header + 2 * sizeof(mixnet_address) + 2 * sizeof(uint16_t), data_size);

                        mixnet_address** best_paths = node->global_best_path[header->dst_address];
                        mixnet_packet* final_data_packet = initialize_DATA_packet(best_paths, header->dst_address, header->src_address, data, data_size);
                        //find hope index and route length
                        mixnet_packet_routing_header* new_header = (mixnet_packet_routing_header*) &final_data_packet->payload;

                        // print_routing_header(new_header);

                        uint16_t nextport = find_next_port(new_header, node);
                        
                        assert(nextport != INVALID_MIXADDR);
                        mixnet_send(handle, nextport, final_data_packet);
                        
                    } else {
                        // printf("DATA Received on!\n");
                        // print_routing_header(header);
                        //i've reached -> am done, thanks. send to my own user
                        if (node->my_addr == header->dst_address){
                            assert(header->hop_index == header->route_length);
                            // printf("Routed Successfully Sending to Node#%d's User\n", node->my_addr);
                            mixnet_send(handle, node->num_neighbors, packet_buffer);
                            sent_to_users++;
                        }
                        else {
                            //else, send to next one in the route_length and increment hop_idx
                            header->hop_index ++;
                            // printf("Hop Index incremented! \n");
                            // print_routing_header(header);
                            assert(header->hop_index > 0);
                            uint16_t nextport = find_next_port(header, node);
                            assert(nextport != INVALID_MIXADDR);
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

                // printf("Received Data/Ping Packet! \n");
                }

            }
        }
    }
    // printf("================NODE #%d FINISH RUN=================\n", node->my_addr);
    // // print_graph(node);
    // printf("Node[#%d] Stats: \n | Received User Packets: %d  | Sent To User: %d, Node[#%d] stopped running \n", node->my_addr, num_user_packets, sent_to_users, node->my_addr);
    // print_node(node, false);
}

uint16_t find_next_port(mixnet_packet_routing_header* routing_header, struct Node* node){

    // Print Cur Hop and Mixnet Address
    // Print Next_Hop and Mixnet Address
    uint16_t hop_idx = routing_header->hop_index;
                    //can i just do this?
    mixnet_address nexthop;

    if (hop_idx == routing_header->route_length){
        // printf("NEXT ONE IS DEST!\n");
        nexthop = routing_header->dst_address;
    } else{
        nexthop = routing_header->route[hop_idx];
    }
     
    // printf("[FIND NXT PORT] Node: %d,  Addr :%d\n", node->my_addr, nexthop);
    for (uint16_t i = 0; i < node->num_neighbors; i++){
        if (node->neighbors_addy[i] == nexthop){
            return i;
        }
    }
    // printf("[ERROR] Node: %d,  Addr :%d\n", node->my_addr, nexthop);
    assert(false);
    return INVALID_MIXADDR;
}


void dijkstra(struct Node * node, bool verbose){
    uint32_t d_start = get_time_in_ms();
    // printf("Running Dijkstra's Algorithm! \n");


    for (uint16_t i = 0; i < (1<<16)-1; i++) {
        node->visited[i] = false;
        node->distance[i] = UINT16_MAX;
        node->prev_neighbor[i] = INVALID_MIXADDR; //? question mark? 
    }
    //run through the old graph, for every neighbor node that's not myself

    node->distance[node->my_addr] = 0; // distance to self == 0
    node->visited[node->my_addr] = false; // node->visited self
    mixnet_address smallestindex = node->my_addr;

    while (node->visitedCount < node->total_neighbors_num + 1) {
        //TODO: find min value from everything in index and visit it 
        //(node w least distance), visit that shit
        uint16_t min_number = UINT16_MAX;
        for (uint16_t i = 0; i < (1<<16)-1; i ++){
            if (!node->visited[i] && node->distance[i] < min_number){
                smallestindex = i;
                min_number = node->distance[i];
            }
        }
        assert(min_number != UINT16_MAX);
        if (verbose) {
            printf("--------------------------------------------------\n");
            printf("Visiting Node:#%u\n", smallestindex);
        }

        //TODO: explore all edges of curr node and check, update prev neighbor if updated
        for (uint16_t i = 0; i < (1<<8)-1; i ++){
            //means there's edge
            if (node->graph[smallestindex][i] != NULL){

                mixnet_address edge_address = node->graph[smallestindex][i]->neighbor_mixaddr;
                uint16_t edge_cost = node->graph[smallestindex][i]->cost;

                // if (verbose) printf("we're visiting neighbor %u with cost %u\n", edge_address, edge_cost);

                //doesn't matter if i've been there before
                uint16_t final_cost = 0;
                final_cost = edge_cost + node->distance[smallestindex];
                if (final_cost < node->distance[edge_address]){
                    node->distance[edge_address] = final_cost;
                    node->prev_neighbor[edge_address] = smallestindex;
                }
            }

            else {
                break;
            }
        }


        node->visited[smallestindex] = true;
        node->visitedCount ++;
    }

    // if (verbose) {
    //     printf("[DIJKSTRA END] we've looped this many times %u\n", node->visitedCount);
    //     printf("this is actually how many neighbors we have %u\n", node->total_neighbors_num);

    //     printf("distance and visited array: ");
    //     for (uint16_t i = 0; i < (1<<16)-1; ++i) {
    //         if (node->distance[i] != UINT16_MAX) {
    //             char* message = node->visited[i] ? "True" : "False";
    //             printf("index is %u, distance is %u, did i visit this %s \n",i, node->distance[i],message);
    //         }
    //     }
    //     printf("\n");
    // }

    //TODO: build shortest paths from our distance and state graph


    for (uint16_t i = 0; i < (1<<16)-1; i ++){
        if (node->distance[i] != UINT16_MAX && i != node->my_addr){
            // if (verbose) printf("this is where I'm going %u \n", i);
            assert(node->visited[i]);
            construct_shortest_path(i, node, node->prev_neighbor);
        }
    }

    if (verbose){
        printf("Dijkstra's took %u ms! \n", get_time_in_ms() - d_start);
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
    mixnet_packet_lsa *lsa_header = (mixnet_packet_lsa *)&LSA_packet->payload;
    // memcpy((void *)lsa_header, (void *)LSA_packet->payload,  sizeof(mixnet_packet_lsa));
    assert(lsa_header->neighbor_count > 0);
    mixnet_lsa_link_params* nb_link_params = (mixnet_lsa_link_params *)&lsa_header->links;
    
    // Update Graph based on my state
    for (uint16_t i = 0; i < lsa_header->neighbor_count; i++) {
        // make this struct to stuff in
        mixnet_address node_addr = lsa_header->node_address;
        mixnet_address neighbor_addr = nb_link_params[i].neighbor_mixaddr;
        uint16_t cost = nb_link_params[i].cost;

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
    for (uint16_t i = 0; i < node->num_neighbors; i++) {
        if (!node->neighbors_blocked[i] && i != sender_port){
            mixnet_packet *temp = malloc(LSA_packet->total_size);
            memcpy(temp, LSA_packet, LSA_packet->total_size);
            // printf("Sending out FLOOD to NB_INDX:%u | addr:%u \n", i, (unsigned int)node->neighbors_addy[i]);
            if (!mixnet_send(handle, i, temp)){
                exit(1);
            } //TODO error_handling
        }
    }
    
}

// Does not reach this function at all
bool receive_STP(struct Node * currNode, uint8_t port, mixnet_packet* stp_packet, void* handle){
    // printf("\n[Received STP packet!]\n");
    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); 
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));

    if (currNode->neighbors_addy[port] == INVALID_MIXADDR) {
        currNode->neighbors_addy[port] = update->node_address;
        currNode->neighbors_blocked[port] = false; // unblock
    }

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

        // currNode->neighbors_blocked[old_root] = 
    }
    // BLOCK LOGIC
    // Parent is unblocked
    if (update->node_address == currNode->next_hop){
        currNode->neighbors_blocked[port] = false; 
    }

    // Unblock potential children 
    else if (update->root_address == currNode->root_addr &&
            update->path_length == currNode->path_len + 1){
            // printf("Node #%d, blocking my potential Parent, who is strictly worse \n", currNode->my_addr);
        currNode->neighbors_blocked[port] = false;
    } else {
        currNode->neighbors_blocked[port] = true;
    }
        
    // free(update);
    return true;
}


// TODO : add print ROUTE
void print_routing_header(mixnet_packet_routing_header* header){
    printf("Printing Header: Route_Len:%d, Hop_Index:%d \n",  header->route_length, header->hop_index);
    printf("Src:%d \n", header->src_address);

    for (uint16_t i = 0; i < header->route_length; i++){
        printf("Route Index[%d]: %d \n", i, header->route[i]);
    }

    printf("DEST:%d \n", header->dst_address);
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
    for (uint16_t i = 0; i < node->num_neighbors; i++) {
        const char* value = (node->neighbors_blocked[i]) ? "Yes" : "No";
        printf("Neighbor Index #%d | Address: %d | Blocked: %s | \n", i, node->neighbors_addy[i], value);
    }
    if (verbose) print_graph(node);
    printf("--------------Printing Node [#%d] Complete!--------------\n",node->my_addr);
}
// Prints out graph of node
void print_graph(struct Node* node) {
    printf("_____________ [GRAPH] of Node [#%d]! _____________  \n", node->my_addr);
    for (uint16_t i = 0; i < node->num_neighbors; i++) {
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
    for (uint16_t i = 0; i < (1<<16)-1; i++) {
        if (node->global_best_path[i] != NULL){
            // Loop through each path for a given destination
            for (uint16_t j = 0; j < (1<<8); j++) {
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

void print_globalgraph(struct Node* node) {
    printf("============ globalpaths of Src Node [#%d]! ============== \n", node->my_addr);
    for (uint16_t i = 0; i < (1<<16)-1; i++) {
        if (node->graph[i] != NULL){
            // Loop through each path for a given destination
            for (uint16_t j = 0; j < (1<<8); j++) {
                // Mixnet Addres of next_hop 
                if (node->graph[i][j] != NULL) {
                    mixnet_address current_path = node->graph[i][j]->neighbor_mixaddr;
                    printf("[%d] NEXT [%d] \n", i, current_path);
                    // printf("-------------------\n");
                }
                else {
                    break;
                }
                // Make sure the path is not NULL before dereferencing
                //printf("Port Idx [%d]: MixNetAddress:#%u\n", j, current_path);
            }
        }
        
        // printf("-------- [END] of Mixnet_addr:%d--------\n", i);
    }
    printf("============ globalpaths of Node [#%d] Complete! ==============\n", node->my_addr);
}

