#include "defrag.h"

PacketAssembler* create_assembler(int packet_id, int total_size) {
    PacketAssembler* assembler = (PacketAssembler*)malloc(sizeof(PacketAssembler));
    if (assembler == NULL) return NULL;

    assembler->packet_id = packet_id;
    assembler->total_size_expected = total_size;
    assembler->total_received_bytes = 0;
    assembler->fragment_count = 0;
    assembler->last_fragment_seen = false;
    assembler->fragment_list_head = NULL;
    assembler->last_seen_timestamp = time(NULL);
    
    return assembler;
}

void free_assembler(PacketAssembler* assembler) {
    if (assembler == NULL) 
        return;

    FragmentNode* curr = assembler->fragment_list_head;
    while (curr != NULL) {
        FragmentNode* to_free = curr;
        curr = curr->next;
        free(to_free->data); 
        free(to_free);
    }
    
    free(assembler);
}

int assembler_add_fragment(PacketAssembler* assembler, int offset, const char* data, int length, bool is_last_fragment) {

    if (offset < 0) {
         printf("Skipping fragment for packet %d (Invalid offset: %d).\n", assembler->packet_id, offset);
         return FRAGMENT_INVALID;
    }
    if (offset + length > assembler->total_size_expected) {
        printf("Skipping fragment for packet %d (Fragment at offset %d, length %d exceeds total size %d).\n",assembler->packet_id, offset, length, assembler->total_size_expected);
        return FRAGMENT_INVALID;
    }

    FragmentNode* new_frag = (FragmentNode*)malloc(sizeof(FragmentNode));
    if (new_frag == NULL) 
        return FRAGMENT_INVALID;
    
    new_frag->offset = offset;
    new_frag->length = length;
    new_frag->data = (char*)malloc(length + 1); 
    if (new_frag->data == NULL) {
        free(new_frag);
        return FRAGMENT_INVALID;
    }
    memcpy(new_frag->data, data, length);
    new_frag->data[length] = '\0';
    new_frag->next = NULL;

    FragmentNode* curr = assembler->fragment_list_head;
    FragmentNode* prev = NULL;

    while (curr != NULL && curr->offset < offset) {
        prev = curr;
        curr = curr->next;
    }
    if (curr != NULL && curr->offset == offset && curr->length == length) {
        free(new_frag->data);
        free(new_frag);
        return FRAGMENT_DUPLICATE;
    }

    if (prev != NULL && (prev->offset + prev->length) > offset) {
        printf("Skipping fragment for packet %d (Fragment at offset %d overlaps with existing fragment at offset %d).\n",
               assembler->packet_id, offset, prev->offset);
        free(new_frag->data);
        free(new_frag);
        return FRAGMENT_INVALID;
    }

    if (curr != NULL && (offset + length) > curr->offset) {
        printf("Skipping fragment for packet %d (Fragment at offset %d overlaps with existing fragment at offset %d).\n",
               assembler->packet_id, offset, curr->offset);
        free(new_frag->data);
        free(new_frag);
        return FRAGMENT_INVALID;
    }

    if (is_last_fragment && assembler->last_fragment_seen) {
        printf("Skipping fragment for packet %d (LFF already received, this is a new invalid fragment).\n", assembler->packet_id);
        free(new_frag->data);
        free(new_frag);
        return FRAGMENT_INVALID;
    }

    FragmentNode* node_to_check_from = NULL;
    if (prev == NULL) {
        new_frag->next = assembler->fragment_list_head;
        assembler->fragment_list_head = new_frag;
        node_to_check_from = assembler->fragment_list_head; 
    } else {
        new_frag->next = curr;
        prev->next = new_frag;
        node_to_check_from = prev; 
    }
    assembler->total_received_bytes += length;
    assembler->fragment_count++;
    
    if (is_last_fragment) {
        assembler->last_fragment_seen = true;
    }

    while (node_to_check_from != NULL && node_to_check_from->next != NULL) {
        FragmentNode* next_node = node_to_check_from->next;
        
        if ((node_to_check_from->offset + node_to_check_from->length) == next_node->offset) {
            int new_len = node_to_check_from->length + next_node->length;
            char* new_data = (char*)realloc(node_to_check_from->data, new_len + 1);
            
            if (new_data) {
                memcpy(new_data + node_to_check_from->length, next_node->data, next_node->length);
                new_data[new_len] = '\0';
                
                node_to_check_from->data = new_data;
                node_to_check_from->length = new_len;
                
                node_to_check_from->next = next_node->next;
                free(next_node->data);
                free(next_node);
                
                assembler->fragment_count--;
            } else {
                node_to_check_from = node_to_check_from->next;
            }
        } else {
            node_to_check_from = node_to_check_from->next;
        }
    }

    return FRAGMENT_ADDED;
}


bool assembler_is_complete(PacketAssembler* assembler) {
    if (assembler->last_fragment_seen &&
        assembler->total_received_bytes == assembler->total_size_expected &&
        assembler->fragment_list_head != NULL &&
        assembler->fragment_list_head->next == NULL &&
        assembler->fragment_list_head->offset == 0) 
    {
        return true;
    }
    return false;
}

char* assembler_get_assembled_data(PacketAssembler* assembler) {
    if (!assembler_is_complete(assembler) || assembler->fragment_list_head == NULL) {
        return NULL;
    }
    
    FragmentNode* head = assembler->fragment_list_head;
    
    char* full_data = (char*)malloc(head->length + 1);
    if (full_data == NULL) return NULL;
    
    memcpy(full_data, head->data, head->length);
    full_data[head->length] = '\0';
    
    return full_data; 
}



void system_init(DefragmenterSystem* system) {
    system->head = NULL; 
    memset(&system->stats, 0, sizeof(SystemStats)); 
    system->current_packet_count = 0;
}

PacketAssembler* system_find_packet(DefragmenterSystem* system, int id) {
    PacketNode* curr = system->head;
    while (curr != NULL) {
        if (curr->assembler->packet_id == id) {
            return curr->assembler;
        }
        curr = curr->next;
    }
    return NULL;
}

void system_remove_packet(DefragmenterSystem* system, int id) {
    PacketNode* curr = system->head;
    PacketNode* prev = NULL;
    while (curr != NULL) {
        if (curr->assembler->packet_id == id) {
            if (prev == NULL) system->head = curr->next;
            else prev->next = curr->next;
            free_assembler(curr->assembler);
            free(curr);
            system->current_packet_count--;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

int system_register_packet(DefragmenterSystem* system, int id, int packet_size_in_bytes) {

    if (packet_size_in_bytes <= 0) {
        printf("ERROR: Packet size must be greater than 0.\n");
        return -1;
    }
    if (packet_size_in_bytes > MAX_PACKET_SIZE_BYTES) {
        printf("ERROR: Packet size %d exceeds system limit of %d bytes.\n", 
               packet_size_in_bytes, MAX_PACKET_SIZE_BYTES);
        return -1;
    }

    if (system_find_packet(system, id) != NULL) {
        printf("ERROR: Packet %d is already being assembled.\n", id);
        return -1;
    }
    if (system->current_packet_count >= MAX_PACKETS_IN_SYSTEM) {
        printf("ERROR: System is full (%d / %d). Cannot add new packet %d.\n",
               system->current_packet_count, MAX_PACKETS_IN_SYSTEM, id);
        return -1;
    }

    PacketAssembler* assembler = create_assembler(id, packet_size_in_bytes);
    if (assembler == NULL) return -1;

    PacketNode* new_node = (PacketNode*)malloc(sizeof(PacketNode));
    if (new_node == NULL) {
        printf("ERROR: Could not create list node. Out of memory.\n");
        free_assembler(assembler);
        return -1;
    }
    
    new_node->assembler = assembler;
    new_node->next = system->head;
    system->head = new_node;
    system->current_packet_count++;
    
    printf("--- Packet %d registered. Total size: %d bytes. ---\n", id, packet_size_in_bytes);
    return 0; 
}

int system_add_fragment(DefragmenterSystem* system, int id, int offset, const char* data, bool is_last_fragment) {
    system->stats.total_fragments_processed++;
    
    PacketAssembler* assembler = system_find_packet(system, id);

    if (assembler == NULL) {
        printf("Skipping fragment for packet %d (Packet not registered).\n", id);
        system->stats.total_invalid_fragments++;
        return -1;
    }

    assembler->last_seen_timestamp = time(NULL);
    int length = strlen(data); 

    int result = assembler_add_fragment(assembler, offset, data, length, is_last_fragment);

    if (result == FRAGMENT_DUPLICATE) {
        system->stats.total_duplicates_discarded++;
    } 
    else if (result == FRAGMENT_INVALID) {
        system->stats.total_invalid_fragments++;
    }

    if (assembler_is_complete(assembler)) {
        printf("\n");
        char* full_packet = assembler_get_assembled_data(assembler);
        if (full_packet) {
            printf("--- PACKET %d COMPLETE ---\n%s\n------------------------\n", 
                   assembler->packet_id, full_packet);
            free(full_packet);
        }
        
        system->stats.packets_completed++;
        system_remove_packet(system, id);
    }
    return 0;
}

int system_get_fragment_count(DefragmenterSystem* system) {
    int total = 0;
    PacketNode* curr = system->head;
    while (curr != NULL) {
        total += curr->assembler->fragment_count; 
        curr = curr->next;
    }
    return total;
}

int system_get_packet_count(DefragmenterSystem* system) {
    return system->current_packet_count;
}

void system_show_stats(DefragmenterSystem* system) {
    printf("\n--- SYSTEM STATISTICS ---\n");
    printf("Total Fragments Processed: %ld\n", system->stats.total_fragments_processed);
    printf("Packets Completed: %ld\n", system->stats.packets_completed);
    printf("Packets Timed Out: %ld\n", system->stats.total_packets_timed_out);
    printf("Fragments Discarded (Duplicate/Overlap): %ld\n", system->stats.total_duplicates_discarded);
    printf("Fragments Discarded (Invalid/Bounds): %ld\n", system->stats.total_invalid_fragments);
    printf("Packets in Reassembly (Current): %d / %d\n", 
           system_get_packet_count(system), MAX_PACKETS_IN_SYSTEM);
    printf("Fragments in Reassembly (Current): %d\n", system_get_fragment_count(system));
    printf("--------------------------\n");
}

void system_print_packet_status(DefragmenterSystem* system, int packet_id) {
    PacketAssembler* assembler = system_find_packet(system, packet_id);

    if (assembler == NULL) {
        printf("\n--- STATUS FOR PACKET %d ---\n", packet_id);
        printf("  Packet not found (it may be complete or not yet started).\n");
        printf("-----------------------------\n");
        return;
    }

    printf("\n--- STATUS FOR PACKET %d ---\n", packet_id);
    printf("  State: In Progress\n");
    printf("  Total Size: %d bytes\n", assembler->total_size_expected);
    printf("  Received Bytes: %d / %d\n", assembler->total_received_bytes, assembler->total_size_expected);
    printf("  Fragments Held: %d\n", assembler->fragment_count); 
    printf("  Last Fragment Flag (LFF): %s\n", assembler->last_fragment_seen ? "SEEN" : "*** MISSING ***");
    printf("  Time remaining until timeout: %.0f seconds\n",
           PACKET_TIMEOUT_SECONDS - difftime(time(NULL), assembler->last_seen_timestamp));
    
    printf("  Fragment List (Sorted by Offset):\n");
    FragmentNode* frag = assembler->fragment_list_head;
    if (frag == NULL) {
        printf("    (None)\n");
    }
    while (frag != NULL) {
        printf("    Offset: %-5d | Length: %-5d | Data: \"%.20s\"\n", 
               frag->offset, frag->length, frag->data);
        frag = frag->next;
    }
    printf("-----------------------------\n");
}

void system_cleanup(DefragmenterSystem* system) {
    PacketNode* curr = system->head;
    while (curr != NULL) {
        PacketNode* to_free = curr;
        curr = curr->next;
        free_assembler(to_free->assembler);
        free(to_free);
    }
    system->head = NULL;
    system->current_packet_count = 0;
}

void system_prune_timeouts(DefragmenterSystem* system) {
    PacketNode* curr = system->head;
    PacketNode* prev = NULL;
    time_t now = time(NULL);

    while (curr != NULL) {
        double time_elapsed = difftime(now, curr->assembler->last_seen_timestamp);
        if (time_elapsed > PACKET_TIMEOUT_SECONDS) {
            printf("\n--- PACKET %d TIMED OUT (%.0f seconds) ---\n", 
                   curr->assembler->packet_id, time_elapsed);
            
            system->stats.total_packets_timed_out++;
            PacketNode* to_free = curr; 

            if (prev == NULL) system->head = curr->next;
            else prev->next = curr->next;
            
            curr = curr->next;

            free_assembler(to_free->assembler);
            free(to_free);
            system->current_packet_count--;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}