
    bool visited[1<<16];
    //lowest distance at each side
    uint16_t* distance[1<<16];
    //for each node, update the parent
    mixnet_address* prev_neighbor[1<<16];

    for (int i = 0; i < (1<<16); i++) {
        visited[i] = false;
        distance[i] = NULL;
        prev_neighbor[i] = NULL; // 
    }

    int visitedCount = 0;
    //source, set as 0
    distance[node->my_addr] = (uint16_t *) malloc(sizeof(uint16_t)); 
    *distance[node->my_addr] = 0; // distance to self == 0
    visited[node->my_addr] = true; // visited self
    mixnet_address curr_node = node->my_addr; 

    printf("Starting Dijkstra's Here @ Node:#%d \n", node->my_addr);
    // should prev of root to -1
    print_node(node);
    int cnts = 0;
    while (visitedCount < node->total_neighbors_num){
        // Check all possible edges from curr_node
        for (int j = 0; j < (1<<8); j++){
            // if nb_j exists
            if (node->graph[curr_node][j] == NULL) {break;}
            
            if (node->graph[curr_node][j] != NULL){
                    // Unpacking values for nb_node
                  mixnet_address nb_node = node->graph[curr_node][j]->neighbor_mixaddr;
                  uint16_t cur_cost = node->graph[curr_node][j]->cost; 

                  if (visited[nb_node]) {
                    continue; 
                  }
                  
                  uint16_t final_cost = 0;
                  
                  if (distance[curr_node] != NULL){
                    final_cost = cur_cost + *distance[curr_node];
                  }
                  else {
                    // only for first edge
                    final_cost = cur_cost;
                  }
                  assert(final_cost != 0);
                  
                  // Relaxation of weights : check for nb_node, if smaller then update
                  if (distance[nb_node] != NULL && final_cost < *distance[nb_node]){
                    *distance[nb_node] = final_cost; // UPDATE DISTANCE

                    if (prev_neighbor[nb_node] == NULL){
                        prev_neighbor[nb_node] = (mixnet_address*) malloc(sizeof(mixnet_address));
                        *prev_neighbor[nb_node] = curr_node;
                    }
                    else {
                        *prev_neighbor[nb_node] = curr_node;
                    }
                  }
                  
                  else if (distance[nb_node] == NULL){
                    // Initialize to 
                    distance[nb_node] = (mixnet_address *) malloc(sizeof(mixnet_address));
                    *distance[nb_node] = final_cost; // UPDATE DISTANCE

                    prev_neighbor[nb_node] = (mixnet_address*) malloc(sizeof(mixnet_address));
                    *prev_neighbor[nb_node] = curr_node;
                  }
                //   printf("")
            }
        }

        printf("Dijkstra's Curr_Node : #%d \n", curr_node);

        // check through to find smallest thing in frontier
        uint16_t min_distance = UINT16_MAX;
        mixnet_address smallestindex = UINT16_MAX;

        for (int i = 0; i < (1<<16); i++) {
            if (distance[i] == NULL){
                continue;
            }
            if (visited[i]){
                continue;
            }
            if (*distance[i] < min_distance) {
                cnts ++ ;
                printf("\n UPDATING ... min_distance from:%d to %d \n", min_distance , *distance[i]);
                min_distance = *distance[i];
                smallestindex = i;
            }
        }  

        printf("Number of updates : %d \n", cnts);

        //why the fuck is this failing
        
        assert(min_distance != UINT16_MAX);
        assert(smallestindex != UINT16_MAX);
        
        //should have smallest thing in frontier rn in smallest index. visit it.
        // update the final list of shortest paths (appending by copying over)
        // get the previous list thing, update all the way in reverse order

        printf("Adding paths now!\n");
        // assert(construct_shortest_path(smallestindex, node, prev_neighbor));

        if (prev_neighbor[smallestindex] == NULL){
            //if there's nothing at the previous neighbor, just append my current source to it (needs to be last thing)
            //like where source is from
            node->global_best_path[smallestindex][0] = (mixnet_address*) malloc(sizeof(mixnet_address));
            *node->global_best_path[smallestindex][0] = node->my_addr;
        }
        else {
            mixnet_address prev =  *prev_neighbor[smallestindex];
            node->global_best_path[smallestindex][0] = (mixnet_address*) malloc(sizeof(mixnet_address));
            *node->global_best_path[smallestindex][0] = prev;
            // xb : -1 ? for for out of bounds
            for (int j = 0; j < (1 << 8) - 1; j ++) {
                // TODO : SEGFAULT
                if (node->global_best_path[prev][j] == NULL){
                    printf("Breaking since we reached end of prev's path\n");
                    break;
                }
                mixnet_address prev_addr = *node->global_best_path[prev][j];

                node->global_best_path[smallestindex][j + 1] = (mixnet_address*) malloc(sizeof(mixnet_address));
                *node->global_best_path[smallestindex][j + 1] = prev_addr;
            }
        }
        // update visitedCount
        visitedCount ++;
        visited[smallestindex] = true;
        // update curr_node to that node
        curr_node = smallestindex;
        printf("Updating next_node to %d", curr_node);
        // repeat until visitedCount == total_neighbors_num
    }

    assert(visitedCount == node->total_neighbors_num);