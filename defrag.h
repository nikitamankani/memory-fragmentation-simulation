#ifndef DEFRAG_H
#define DEFRAG_H

#include <stdio.h>    
#include <stdlib.h>   
#include <string.h>   
#include <stdbool.h>  
#include <time.h>     


#define FRAGMENT_OK 0
#define FRAGMENT_DUPLICATE 1
#define FRAGMENT_INVALID 2
#define FRAGMENT_ADDED 3


#define MAX_PACKET_SIZE_BYTES 65535
#define MAX_PACKETS_IN_SYSTEM 128     
#define PACKET_TIMEOUT_SECONDS 60   



typedef struct {
    long total_fragments_processed;
    long packets_completed;
    long total_duplicates_discarded;
    long total_invalid_fragments;
    long total_packets_timed_out;
} SystemStats;

typedef struct FragmentNode {
    int offset;
    int length;
    char* data;
    struct FragmentNode* next;
} FragmentNode;

typedef struct {
    int packet_id;
    int total_size_expected;  
    int total_received_bytes;
    int fragment_count;     
    bool last_fragment_seen;
    
    FragmentNode* fragment_list_head;
    
    time_t last_seen_timestamp;
    
} PacketAssembler;

typedef struct PacketNode {
    PacketAssembler* assembler;
    struct PacketNode* next;
} PacketNode;

typedef struct {
    PacketNode* head;
    SystemStats stats;
    int current_packet_count;
} DefragmenterSystem;

PacketAssembler* create_assembler(int packet_id, int total_size);
void free_assembler(PacketAssembler* assembler);
int assembler_add_fragment(PacketAssembler* assembler, int offset, const char* data, int length, bool is_last_fragment);
bool assembler_is_complete(PacketAssembler* assembler);
char* assembler_get_assembled_data(PacketAssembler* assembler);

void system_init(DefragmenterSystem* system);
int system_register_packet(DefragmenterSystem* system, int id, int packet_size_in_bytes);
int system_add_fragment(DefragmenterSystem* system, int id, int offset, const char* data, bool is_last_fragment);

void system_show_stats(DefragmenterSystem* system);
int system_get_fragment_count(DefragmenterSystem* system);
int system_get_packet_count(DefragmenterSystem* system);
void system_print_packet_status(DefragmenterSystem* system, int packet_id);
void system_cleanup(DefragmenterSystem* system);
void system_prune_timeouts(DefragmenterSystem* system);

#endif 