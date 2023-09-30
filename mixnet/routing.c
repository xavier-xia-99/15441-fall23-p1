#include "node.h"
#include "connection.h"
#include "packet.h"
#include "utils.h"
#include "routing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
    //run through the old graph, for every neighbor node that's not myself

    while (node->visitedCount < node->total_nodes + 1) {
        //TODO: find min value from everything in index and visit it 
        //(node w least distance), visit that shit
        uint16_t min_number = UINT16_MAX;
        for (uint16_t i = 0; i < (1<<16)-1; i ++){
            if (!node->visited[i] && node->distance[i] < min_number){
                node->smallestindex = i;
                min_number = node->distance[i];
            }
        }
        assert(min_number != UINT16_MAX);
        if (verbose) {
            printf("--------------------------------------------------\n");
            printf("Visiting Node:#%u\n", node->smallestindex);
        }

        //TODO: explore all edges of curr node and check, update prev neighbor if updated
        for (uint16_t i = 0; i < (1<<8)-1; i ++){
            //means there's edge
            if (node->graph[node->smallestindex][i] != NULL){

                mixnet_address edge_address = node->graph[node->smallestindex][i]->neighbor_mixaddr;
                uint16_t edge_cost = node->graph[node->smallestindex][i]->cost;

                // if (verbose) printf("we're visiting neighbor %u with cost %u\n", edge_address, edge_cost);

                //doesn't matter if i've been there before
                uint16_t final_cost = 0;
                final_cost = edge_cost + node->distance[node->smallestindex];
                if (final_cost < node->distance[edge_address]){
                    node->distance[edge_address] = final_cost;
                    node->prev_neighbor[edge_address] = node->smallestindex;
                }
            }
            else {
                break;
            }
        }


        node->visited[node->smallestindex] = true;
        node->visitedCount ++;
    }

    // if (verbose) {
    //     printf("[DIJKSTRA END] we've looped this many times %u\n", node->visitedCount);
    //     printf("this is actually how many neighbors we have %u\n", node->total_nodes);

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

    assert(node->visitedCount == node->total_nodes + 1);
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
            node->total_nodes++;
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
