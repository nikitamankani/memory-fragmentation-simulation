#include "defrag.h"
#include <stdio.h>

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main() {
    DefragmenterSystem system;
    system_init(&system);
    
    setvbuf(stdout, NULL, _IONBF, 0);
    
    int choice = 0;
    
    while (choice != 6) { 
        system_prune_timeouts(&system);
        
        printf("\n--- Packet Defragmenter Menu ---\n");
        printf("1. Register New Packet (by Size)\n");
        printf("2. Add Fragment (by Offset)\n");
        printf("3. Show System Statistics\n");
        printf("4. Display No. of Fragments in System\n");
        printf("5. Print Specific Packet Status\n"); 
        printf("6. Quit\n");                         
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            clear_input_buffer(); 
            continue;
        }
        clear_input_buffer(); 

        switch (choice) {
            case 1: {
                int id, size_in_bytes;
                printf("  Enter Packet ID to register: ");
                
                if (scanf("%d", &id) != 1) {
                    printf("    ERROR: Invalid input. Packet ID must be a number.\n");
                    clear_input_buffer();
                    break; 
                }
                clear_input_buffer();
                
                printf("  Enter Total Packet Size (in bytes): ");
                
                if (scanf("%d", &size_in_bytes) != 1) {
                    printf("    ERROR: Invalid input. Packet Size must be a number.\n");
                    clear_input_buffer();
                    break; 
                }
                clear_input_buffer();
                
                
                system_register_packet(&system, id, size_in_bytes);
                break;
            }
            
            case 2: {
                int id, offset;
                char data_buffer[2048];
                char lff_input;
                bool is_last;

                printf("  Enter Packet ID: ");
                
                if (scanf("%d", &id) != 1) {
                    printf("    ERROR: Invalid input. Packet ID must be a number.\n");
                    clear_input_buffer();
                    break; 
                }
                clear_input_buffer();

                printf("  Enter Fragment Offset (e.g., 0, 1024, ...): ");
                
                if (scanf("%d", &offset) != 1) {
                    printf("    ERROR: Invalid input. Offset must be a number.\n");
                    clear_input_buffer();
                    break; 
                }
                clear_input_buffer();
                
                
                
                while (1) {
                    printf("  Is this the Last Fragment? (y/n): ");
                    scanf(" %c", &lff_input);
                    clear_input_buffer();

                    if (lff_input == 'y' || lff_input == 'Y') {
                        is_last = true;
                        break; 
                    } else if (lff_input == 'n' || lff_input == 'N') {
                        is_last = false;
                        break;
                    } else {
                        printf("    Invalid input. Please enter 'y' or 'n'.\n");
                    }
                }

                printf("  Enter Data: ");
                if (fgets(data_buffer, sizeof(data_buffer), stdin)) {
                    data_buffer[strcspn(data_buffer, "\n")] = '\0';
                }
                
                if (strlen(data_buffer) == 0) {
                    printf("    Invalid input. Data cannot be empty.\n");
                    break;
                }
                
                system_add_fragment(&system, id, offset, data_buffer, is_last);
                break;
            }

            case 3:
                system_show_stats(&system);
                break;

            case 4: {
                int count = system_get_fragment_count(&system);
                printf("\nFragments currently in system: %d\n", count);
                break;
            }

            case 5: { 
                int id_to_print;
                printf("  Enter Packet ID to print status for: ");
                
                
                if (scanf("%d", &id_to_print) != 1) {
                    printf("    ERROR: Invalid input. Packet ID must be a number.\n");
                    clear_input_buffer();
                    break; 
                }
                clear_input_buffer();
                
                
                system_print_packet_status(&system, id_to_print);
                break;
            }

            case 6: 
                printf("Cleaning up and exiting\n");
                break;
                
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }

    system_cleanup(&system);
    printf("Goodbye.\n");
    
    return 0;
}