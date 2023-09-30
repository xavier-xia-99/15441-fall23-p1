#include "node.h"

#include "connection.h"
#include "packet.h"

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

uint32_t get_time_in_ms(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000) + (time.tv_nsec / 1000000);
}


uint64_t local_time(void) {
    time_t currentTime;
    struct tm *localTime;

    // Get current time
    currentTime = time(NULL);

    // Convert to local time
    localTime = localtime(&currentTime);

    // Convert struct tm back to time_t (seconds since the epoch)
    time_t localTimeInSeconds = mktime(localTime);

    return (uint64_t)localTimeInSeconds;
}

 
mixnet_packet* initialize_STP_packet(mixnet_address root_address,
                                   uint16_t path_length,
                                   mixnet_address node_address) {
                                    
  mixnet_packet *stp_packet = (mixnet_packet *)malloc(
      sizeof(mixnet_packet) + sizeof(mixnet_packet_stp));

  if (stp_packet == NULL) {
    exit(1);
  }
  // Initialize mixnet_packet fields
  stp_packet->total_size = 18;
  stp_packet->type = PACKET_TYPE_STP;

  // Allocate memory for the mixnet_packet_stp payload
  mixnet_packet_stp *stp_payload =
      (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
  // if (!stp_payload) {
  //   // Handle allocation failure
  //   free(stp_packet);
  //   exit(1);
  // }
  // Initialize mixnet_packet_stp fields
  stp_payload->root_address = root_address;
  stp_payload->path_length = path_length;
  stp_payload->node_address = node_address;

  // Point packet's payload to stp_payload
  memcpy((void *)&stp_packet->payload, (void *)stp_payload,
         sizeof(mixnet_packet_stp));

  return stp_packet;
}

mixnet_packet* initialize_FLOOD_packet(mixnet_address root_address,
                                   uint16_t path_length,
                                   mixnet_address node_address) {
                                    
  mixnet_packet *stp_packet = (mixnet_packet *)malloc(
      sizeof(mixnet_packet) + sizeof(mixnet_packet_stp));

  if (stp_packet == NULL) {
    exit(1);
  }
  // Initialize mixnet_packet fields
  stp_packet->total_size = 12;
  stp_packet->type = PACKET_TYPE_FLOOD;

  // Allocate memory for the mixnet_packet_stp payload
  mixnet_packet_stp *stp_payload =
      (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
  if (!stp_payload) {
    // Handle allocation failure
    free(stp_packet);
    exit(1);
  }

  // Initialize mixnet_packet_stp fields
  stp_payload->root_address = root_address;
  stp_payload->path_length = path_length;
  stp_payload->node_address = node_address;

  // Point packet's payload to stp_payload
  memcpy((void *)&stp_packet->payload, (void *)stp_payload,
         sizeof(mixnet_packet_stp));
  
  return stp_packet;
}

// [DONE?]
mixnet_packet* initialize_LSA_packet(mixnet_address node_addr, uint16_t nb_count, mixnet_address* nb_addrs, uint16_t* nb_costs ){
  
  
  mixnet_packet *LSA_packet = (mixnet_packet *)malloc(sizeof(mixnet_packet) + 4 + 4 * nb_count);

  if (LSA_packet == NULL) {
    printf("damn ran out of memory homie");
    return NULL;
  }

  LSA_packet->total_size = 12 + 4 + 4 * nb_count;
  LSA_packet->type = PACKET_TYPE_LSA;

  // header *h = (header *)&packet->header
  mixnet_packet_lsa* lsa_header = (mixnet_packet_lsa*)&LSA_packet->payload;
  lsa_header->node_address = node_addr;
  lsa_header->neighbor_count = nb_count;

  mixnet_lsa_link_params* nb_links = (mixnet_lsa_link_params*)&lsa_header->links;
  char *ptr = (char*)nb_links;
  
   for (uint16_t i = 0; i < nb_count; ++i) {
      mixnet_lsa_link_params link_params = { nb_addrs[i], nb_costs[i] };
      memcpy(ptr, &link_params, sizeof(mixnet_lsa_link_params));
      ptr += sizeof(mixnet_lsa_link_params);
   }

  return LSA_packet;
}



// TODO : IMPL
mixnet_packet* initialize_DATA_packet(mixnet_address** best_paths, mixnet_address dst_address, mixnet_address src_address, char* data, unsigned long data_size){
    assert(best_paths != NULL);
    assert(dst_address != src_address);
    int path_len = 0;

    for (int i = 0; i < MAX_MIXNET_ROUTE_LENGTH; i++) {
      if (best_paths[i] == NULL) {
          // printf("Ending early for path len");
          break;
      }
      path_len ++;
    }

    assert(path_len > 0);
    assert(*best_paths[path_len - 1] == src_address); // Last one should be src_address

    assert(sizeof(mixnet_packet_routing_header) == 8);
    unsigned long total_size = 12 + sizeof(mixnet_packet_routing_header) + (path_len-1) * sizeof(mixnet_address) + data_size;
    mixnet_packet *DATA_packet = (mixnet_packet *) malloc(total_size);

    DATA_packet->total_size = total_size;
    DATA_packet->type = PACKET_TYPE_DATA;

    mixnet_packet_routing_header *routing_header = (mixnet_packet_routing_header *)&DATA_packet->payload;
    routing_header->dst_address = dst_address;
    routing_header->src_address = src_address;
    routing_header->route_length = path_len - 1;
    routing_header->hop_index = 0;

    // Reverse the path from new_path
    for (int i = 0; i < routing_header->route_length; i++) {
      assert(path_len - i - 2 >= 0); // 
      mixnet_address *addr_ptr = best_paths[path_len - i - 2];
      routing_header->route[i] = *addr_ptr;
    }
    // Copy back the to the end of the route w size of MAX_MIXNET_DATA_SIZE
    memcpy((char*)&routing_header->route + ((routing_header->route_length) * sizeof(mixnet_address)), data, data_size);
    
    // printf("[CHECK] from %u to %u \n",src_address, dst_address);
    // for (int i = 0; i < routing_header->route_length; i++) {
    //   printf("Route %d: %u \n", i, routing_header->route[i]);
    //   // assert(routing_header->route[i]);
    // }
    return DATA_packet;    

}

void reverse_ping(mixnet_packet *PING_packet){
    mixnet_packet_routing_header *routing_header = (mixnet_packet_routing_header *)&PING_packet->payload;
    mixnet_address dest = routing_header->dst_address;
    mixnet_address src = routing_header->src_address;
    // Flip Src and Dest
    routing_header->dst_address = src;
    routing_header->src_address = dest;
    routing_header->hop_index = 0; // Reset the hop_index

    uint16_t start = 0;
    uint16_t end, last;
    end = routing_header->route_length-1;

    // mixnet_address* old = (mixnet_address*)routing_header->route[0];

    while (start < end){
        // Swap elements at start and end
        mixnet_address temp = routing_header->route[start];
        routing_header->route[start] = routing_header->route[end];
        routing_header->route[end] = temp;
        // Move indices inward
        start++;
        end--;
    }
    last = routing_header->route_length;

    // Check that the route is flipped
    // for (uint16_t i = 0; i < routing_header->route_length; i++) {
        // printf("Route%d : %d ", i, routing_header->route[i]);
    // }
    // printf("\n");

    // Access and Flip request
    mixnet_packet_ping* PING_payload = (mixnet_packet_ping*)&routing_header->route[last];
    assert(PING_payload->is_request == true);
    PING_payload->is_request = false;
}
mixnet_packet* initialize_PING_packet(mixnet_address** best_paths, mixnet_address dst_address, mixnet_address src_address){
    assert(best_paths != NULL);
    assert(dst_address != src_address);
    int path_len = 0;

    for (int i = 0; i < MAX_MIXNET_ROUTE_LENGTH; i++) {
      if (best_paths[i] == NULL) {
          // printf("Ending early for path len");
          break;
      }
      path_len ++;
    }

    assert(path_len > 0);
    assert(*best_paths[path_len - 1] == src_address); // Last one should be src_address

    assert(sizeof(mixnet_packet_routing_header) == 8);
    unsigned long total_size = 12 + sizeof(mixnet_packet_routing_header) + (path_len-1) * sizeof(mixnet_address) + sizeof(mixnet_packet_ping);
    mixnet_packet *PING_packet = (mixnet_packet *) malloc(total_size);

    PING_packet->total_size = total_size;
    PING_packet->type = PACKET_TYPE_PING;

    mixnet_packet_routing_header *routing_header = (mixnet_packet_routing_header *)&PING_packet->payload;
    routing_header->dst_address = dst_address;
    routing_header->src_address = src_address;
    routing_header->route_length = path_len - 1;
    routing_header->hop_index = 0;

    // Reverse the path from new_path
    uint16_t i = 0;
    for (i = 0; i < routing_header->route_length; i++) {
      assert(path_len - i - 2 >= 0); // 
      mixnet_address *addr_ptr = best_paths[path_len - i - 2];
      routing_header->route[i] = *addr_ptr;
    }
    // //
    // typedef struct mixnet_packet_ping {
    // bool is_request;                    // true if request, false otherwise
    // uint8_t _pad[1];                    // 1B pad for easier diagramming
    // uint64_t send_time;                 // Sender-populated request time
    // }
    assert(i == routing_header->route_length);
    mixnet_packet_ping* PING_payload = (mixnet_packet_ping*)&routing_header->route[i];
    PING_payload->is_request = true;
    PING_payload->send_time = local_time();

    // printf("[CHECK] from %u to %u \n",src_address, dst_address);
    // for (int i = 0; i < routing_header->route_length; i++) {
    //   printf("Route %d: %u \n", i, routing_header->route[i]);
    //   // assert(routing_header->route[i]);
    // }
    return PING_packet;    
}


// TODO : add support for custom message
void print_STP_packet(mixnet_packet *packet) {
    //  Free-d
    printf("\n--------------------[START OF PACKET]------------------\n");
    printf("PACKET TYPE: %d \n", packet->type);
    printf("Payload: \n");
    switch (packet->type) {
        case PACKET_TYPE_STP:
        case PACKET_TYPE_FLOOD:
        {
            mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); //
            memcpy((void *)update, (void *)packet->payload,
                sizeof(mixnet_packet_stp));

            printf("Printing Packet! \n");
            printf("Root(Node) Address: %d \n", update->root_address);
            printf("Path Length: %d \n", update->path_length);
            printf("Sender(Node) address: %d \n", update->node_address);

            // free(update);
        }
        break;  
    // case PACKET_TYPE_DATA:
    //     mixnet_packet_data *update =
    //         (mixnet_packet_data *)malloc(sizeof(mixnet_packet_data)); //
    //     memcpy((void *)data, (void *)packet->payload,
    //            sizeof(mixnet_packet_data));
    //     printf("Printing Packet! \n");
    //     printf("Root address: %d \n", data->root_address);
    //     printf("Path length: %d \n", data->path_length);
    //     printf("Node address: %d \n", data->node_address);
    //     printf("Data: %s \n", data->data);
    //     break;
    // }
    printf("\n-------------------[END OF PACKET]----------------------\n");
    }
}

char* get_packet_type(mixnet_packet *packet) {
    switch (packet->type) {
        case PACKET_TYPE_STP:
        return "STP";
        case PACKET_TYPE_FLOOD:
        return "FLOOD";
        case PACKET_TYPE_LSA:
        return "LSA";
        case PACKET_TYPE_DATA:
        return "DATA";
        case PACKET_TYPE_PING:
        return "PING";
        default :
        return "ERROR WITH PACKET TYPE!";
    }
}

void print_node_config(const struct mixnet_node_config config){
    printf("--------- Printing Node Config! ---------\n");
    printf("Node Address: %d \n", config.node_addr);
    printf("Root Hello Interval: %d \n", config.root_hello_interval_ms);
    printf("Reelection Interval: %d \n", config.reelection_interval_ms);
    printf("Number of Neighbors: %d \n", config.num_neighbors);
    // printf("Neighbors: \n");
    // for (int i = 0; i < config.num_neighbors; i++) {
    //     printf("Neighbor Node %d: %d \n", i, config.neighbors_addrs[i]);
    // }
    printf("---------Printing Node Config Complete! ---------\n");
}
