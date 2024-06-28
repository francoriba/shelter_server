#pragma once

#include "../lib/alertInfection/include/alertInfection.h"
#include "../lib/cJSON/include/cJSON.h"
#include "../lib/emergNotif/include/emergNotif.h"
#include "../lib/socketSetup/include/socket_setup.h"
#include "../lib/suppliesData/include/supplies_module.h"
#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define BUFFER_521 512
#define BUFFER_256 256
#define BUFFER_64 64
#define DEFAULT_PORT -1
#define MAX_CONNECTIONS 5
#define MAX_CLIENTS 5
#define NUM_SENSORS 4
#define SHARED_MEM_PORTS "/port_shared_memory"
#define ADMIN_USER "ubuntu"
#define SOCK_PATH "/tmp/socket"
#define FIFO_PATH "/tmp/alerts_fifo"
#define LOG_FILENAME "refuge.log"
#define LOG_DIR "/.refuge/"
#define SECONDS_IN_MINUTE 60

/**
 * @file network_structs.h
 * @brief Data structures for TCP clients, UDP clients, and emergency alerts.
 */

/**
 * @struct TCPClientList
 * @brief Structure to hold a list of TCP clients.
 *
 * This structure maintains an array of file descriptors for connected TCP clients
 * and keeps track of the number of clients.
 *
 * @var TCPClientList::client_fds
 * Array of file descriptors for connected clients.
 *
 * @var TCPClientList::num_clients
 * Number of currently connected clients.
 */
typedef struct
{
    int client_fds[MAX_CLIENTS];
    int num_clients;
} TCPClientList;

/**
 * @enum AddressFamily
 * @brief Enumeration for address families (IPv4 or IPv6).
 *
 * This enumeration specifies the address family used by UDP clients.
 *
 * @var AddressFamily::IPv4
 * IPv4 address family.
 *
 * @var AddressFamily::IPv6
 * IPv6 address family.
 */
typedef enum
{
    IPv4,
    IPv6
} AddressFamily;

/**
 * @struct UDPClientData
 * @brief Structure to hold data about each UDP client.
 *
 * This structure contains the socket file descriptor, client address,
 * address length, and address family for a UDP client.
 *
 * @var UDPClientData::sockfd
 * Socket file descriptor.
 *
 * @var UDPClientData::client_addr
 * Client address structure (sockaddr_storage to accommodate both IPv4 and IPv6).
 *
 * @var UDPClientData::addr_len
 * Size of the client address structure.
 *
 * @var UDPClientData::family
 * Address family (IPv4 or IPv6).
 */
typedef struct
{
    int sockfd;                          // Socket file descriptor
    struct sockaddr_storage client_addr; // Client address structure
    socklen_t addr_len;                  // Size of the client address structure
    AddressFamily family;                // Address family (IPv4 or IPv6)
} UDPClientData;

/**
 * @struct UDPClientList
 * @brief Structure to hold the list of UDP clients.
 *
 * This structure maintains an array of UDPClientData structures
 * and keeps track of the number of connected UDP clients.
 *
 * @var UDPClientList::clients
 * Array to store UDP clients.
 *
 * @var UDPClientList::num_clients
 * Number of connected UDP clients.
 */
typedef struct
{
    UDPClientData clients[MAX_CLIENTS]; // Array to store UDP clients
    int num_clients;                    // Number of connected UDP clients
} UDPClientList;

/**
 * @struct EntryAlertsCount
 * @brief Structure to hold data about alerts in each entry point.
 *
 * This structure keeps track of the number of alerts for each entry point
 * (north, south, east, and west).
 *
 * @var EntryAlertsCount::north
 * Number of alerts in the north entry point.
 *
 * @var EntryAlertsCount::south
 * Number of alerts in the south entry point.
 *
 * @var EntryAlertsCount::east
 * Number of alerts in the east entry point.
 *
 * @var EntryAlertsCount::west
 * Number of alerts in the west entry point.
 */
typedef struct
{
    int north;
    int south;
    int east;
    int west;
} EntryAlertsCount;

/**
 * @struct EmergencyInfo
 * @brief Structure to hold data about emergency information.
 *
 * This structure stores information about the last keep-alive message
 * and the last event description.
 *
 * @var EmergencyInfo::last_keepalived
 * Date and time of the last keep-alive message.
 *
 * @var EmergencyInfo::last_event
 * Description of the last event.
 */
typedef struct
{
    char last_keepalived[30]; // To store date and time
    char last_event[100];     // To store event description
} EmergencyInfo;

/**
 * @brief Initializes the server and starts listening for incoming connections.
 */
void start_server(int tcp_port, int udp_port);

/**
 * @brief Creates a Unix domain socket and configures it to listen on the specified path.
 *
 * @param socket_path The path for the Unix domain socket.
 * @param MAX_CONNECTIONS The maximum number of pending connections in the socket's listen queue.
 * @param connection_oriented Indicates whether the socket should be connection-oriented (1) or connectionless (0).
 *
 * @return The file descriptor of the created Unix domain socket.
 */
void handle_unix_socket_activity(int sockfd, const char* client_type, int connection_oriented);

/**
 * @brief Handles activity on a TCP socket, including receiving messages from the client.
 *
 * @param sockfd The file descriptor of the TCP socket.
 * @param addr Pointer to a sockaddr structure containing client address information.
 * @param addrlen Size of the sockaddr structure.
 * @param client_type A string indicating the type of client (e.g., "TCP")
 * @param max_clients The maximum number of clients that can be handled by the server.
 * @return void
 */
void handle_tcp_socket_activity(int client_fd, fd_set* master_fds);

/**
 * @brief Handles activity on a UDP socket.
 *
 * This function receives data from a UDP socket, processes it, and prints the received message
 * along with the client type.
 *
 * @param sockfd The socket file descriptor.
 * @param addr Pointer to the socket address structure.
 * @param addrlen Length of the socket address structure.
 * @param client_type The type of client (e.g., "IPv4", "IPv6", "Unix").
 */
void handle_udp_socket_activity(int sockfd);

/**
 * @brief Cleans up the Unix domain socket file.
 *
 * This function removes the Unix domain socket file specified by the given socket_path.
 * If the removal fails and the error is not due to the file not existing (ENOENT), it prints an error message.
 *
 * @param socket_path The path to the Unix domain socket file.
 */
void cleanup_unix_socket(const char* socket_path);

/**
 * @brief Signal handler for SIGINT (Ctrl+C) to gracefully shut down the server.
 *
 * This function sets the global variable SERVER_RUNNING to 0, indicating that the server should shut down.
 * Additionally, it prints a message to indicate that the server is shutting down.
 *
 * @param signal The signal received (in this case, SIGINT).
 */
void sigint_handler();

/**
 * @brief Accepts a connection on a TCP socket.
 *
 * @param sockfd The socket file descriptor.
 * @param addr Pointer to the socket address structure.
 * @param addrlen Length of the socket address structure.
 * @return int The file descriptor of the accepted connection, or -1 on error.
 */
int accept_tcp_connection(int sockfd, struct sockaddr* addr, socklen_t addrlen);

/**
 * @brief Retrieves an available port by creating a socket and binding it to a port.
 *
 * This function creates a socket, binds it to port 0 (allowing the operating system to
 * assign an available port), retrieves the assigned port, and then closes the socket.
 *
 * @return An available port number assigned by the OS.
 *         On failure, it returns -1 and sets errno accordingly.
 */
int get_available_port();

/**
 * @brief Sets the TCP and UDP ports based on which parameters the user provided/ommited.
 * @param tcp_port A pointer to the TCP port number.
 * @param udp_port A pointer to the UDP port number.
 */
void set_ports(int* tcp_port, int* udp_port);

/**
 * @brief Shares TCP and UDP ports using shared memory.
 *
 * This function creates a shared memory region and writes the provided TCP and UDP ports
 * into this memory region. Clients can access this shared memory region to retrieve the
 * TCP and UDP ports that the server is using.
 *
 * @param tcp_port The TCP port number to be shared.
 * @param udp_port The UDP port number to be shared.
 */
void share_ports_shared_memory(int tcp_port, int udp_port);

/**
 * @brief Retrieves the port number associated with a given socket.
 *
 * This function retrieves the port number associated with the specified socket file descriptor.
 *
 * @param sockfd The socket file descriptor for which the port number is to be retrieved.
 * @return The port number associated with the socket.
 */
int get_selected_port(int sockfd);

/**
 * @brief Receives a JSON object from the client.
 *
 * This function receives a JSON object from the client over the provided socket file descriptor.
 *
 * @param sockfd The socket file descriptor to receive data from.
 * @return A pointer to the cJSON object containing the received JSON data, or NULL if an error occurs or no data is
 * received.
 */
cJSON* receive_tcp_json(int sockfd);

/**
 * @brief Sends a JSON object to the client.
 *
 * This function converts the cJSON object to a JSON string, sends it to the client over the provided socket file
 * descriptor, and frees the memory used by the JSON string.
 *
 * @param sockfd The socket file descriptor to send data to.
 * @param json A pointer to the cJSON object to be sent.
 */
void send_json_to_tcp_client(int sockfd, cJSON* json);

/**
 * @brief Sends a JSON object to the client over UDP.
 *
 * This function converts the cJSON object to a JSON string, sends it to the client over the provided socket file
 * descriptor and client address, and frees the memory used by the JSON string.
 *
 * @param sockfd The socket file descriptor to send data to.
 * @param client_addr Pointer to the sockaddr structure containing client address information.
 * @param client_addrlen Length of the sockaddr structure.
 * @param json_response A pointer to the cJSON object to be sent.
 */
void send_json_to_udp_client(int sockfd, struct sockaddr* client_addr, socklen_t client_addrlen, cJSON* json_response);

/**
 * @brief Receives and processes messages from the client on TCP.
 *
 * @param client_fd The file descriptor of the client socket.
 * @param client_type A string indicating the type of client (e.g., "TCP").
 * @return Returns 1 if the message is successfully received and processed, 0 otherwise.
 */
int check_tcp_clients_messages(int client_fd);

/**
 * @brief Retrieves client information from a UDP client.
 *
 * This function retrieves client IP address and port number from the provided sockaddr_storage structure.
 *
 * @param client_addr Pointer to the sockaddr_storage structure containing client address information.
 * @param client_ip Buffer to store the client IP address.
 * @param ip_buffer_size Size of the buffer to store the client IP address.
 * @param client_port Pointer to an integer to store the client port number.
 */
void get_udp_client_info(struct sockaddr_storage* client_addr, char* client_ip, size_t ip_buffer_size,
                         int* client_port);

/**
 * @brief Receives a JSON object from a UDP client.
 *
 * This function receives a JSON object from the client over the provided socket file descriptor and client address.
 *
 * @param sockfd The socket file descriptor to receive data from.
 * @param client_addr Pointer to the sockaddr_storage structure containing client address information.
 * @param client_addrlen Pointer to the length of the sockaddr structure.
 * @return A pointer to the cJSON object containing the received JSON data, or NULL if an error occurs or no data is
 * received.
 */
cJSON* receive_udp_json(int sockfd, struct sockaddr_storage* client_addr, socklen_t* client_addrlen);

/**
 * @brief Processes messages from the UDP clients.
 *
 * @param sockfd The socket file descriptor.
 * @return Returns 1 if the message is successfully received and processed, 0 otherwise.
 */
int check_udp_clients_messages(int sockfd);

/**
 * @brief Redirects the standard output of the child process to the parent process.
 *
 * @param pid The ID of the child process whose output will be redirected to the parent process.
 *
 * @return This function does not return any value.
 */
void redirect_output_to_parent();

/**
 * @brief Parses command line arguments to extract TCP and UDP ports.
 *
 * This function parses command line arguments to extract TCP and UDP ports.
 * It expects the arguments to be provided in the format '-p tcp <tcp_port>' and '-p udp <udp_port>'.
 * If any of the ports are not specified, they will remain uninitialized (-1).
 *
 * @param argc The number of command line arguments.
 * @param argv An array of strings containing the command line arguments.
 * @param tcp_port A pointer to an integer to store the TCP port.
 * @param udp_port A pointer to an integer to store the UDP port.
 */
void parse_command_line_arguments(int argc, char* argv[], int* tcp_port, int* udp_port);

/**
 * @brief Handles a new TCP connection.
 *
 * This function is responsible for accepting a new TCP connection on the specified
 * TCP socket file descriptor and performing any necessary actions upon successful
 * connection establishment.
 *
 * @param tcp_socket_fd The file descriptor of the TCP socket where the new connection will be accepted.
 * @return None
 */
void handle_new_tcp_connection(int tcp_socket_fd);

/**
 * @brief Converts food and medicine supplies into a JSON object.
 *
 * This function takes food and medicine supply data and creates a JSON object to represent them.
 *
 * @param food_supply A pointer to a FoodSupply structure containing information about food supplies.
 * @param medicine_supply A pointer to a MedicineSupply structure containing information about medicine supplies.
 * @return A pointer to a cJSON object representing the supplies data, or NULL if there's an error.
 */
cJSON* convert_supplies_to_json(FoodSupply* food_supply, MedicineSupply* medicine_supply);

/**
 * @brief Initializes the alert module.
 *
 * This function creates a FIFO (named pipe) for inter-process communication and allocates memory for an array of Sensor
 * structures representing different entry points. It initializes the names of the sensors and returns a pointer to the
 * allocated memory.
 *
 * @return A pointer to an array of Sensor structures representing different entry points, or NULL if there's an error.
 */
Sensor* initiateAlertModule();

/**
 * @brief Checks for alerts in the FIFO and handles them.
 *
 * This function reads any available alerts from the FIFO and prints them to the standard output. It should be called
 * when there is an alert message in the FIFO.
 */
void check_alerts();

/**
 * @brief Cleans up the FIFO by removing it from the file system.
 *
 * This function removes the FIFO file from the file system.
 *
 * @param fifo_path The path to the FIFO file.
 */
void cleanup_fifo(const char* fifo_path);

/**
 * @brief Checks if the FIFO is not empty.
 *
 * This function checks if there are any data available in the FIFO for reading.
 *
 * @param fd The file descriptor of the FIFO.
 * @return 1 if the FIFO is ready for reading (not empty), 0 otherwise.
 */
int fifo_not_empty(int fd);

/**
 * @brief Adds a TCP client to the list of connected clients.
 *
 * This function adds a TCP client represented by the given file descriptor
 * to the list of connected clients. It checks if there is space available
 * in the client list before adding the client.
 *
 * @param client_fd The file descriptor of the TCP client to add.
 * @param tcp_clients_list A pointer to the TCPClientList structure representing the list of connected TCP clients.
 */
void add_tcp_client(int client_fd, TCPClientList* tcp_clients_list);

/**
 * @brief Removes a TCP client from the list of connected clients.
 *
 * This function removes a TCP client represented by the given file descriptor
 * from the list of connected clients. It searches for the client in the list
 * and removes it if found.
 *
 * @param client_fd The file descriptor of the TCP client to remove.
 * @param tcp_clients A pointer to the TCPClientList structure representing the list of connected TCP clients.
 */
void remove_tcp_client(int client_fd, TCPClientList* tcp_clients);

/**
 * @brief Sends a message to all connected TCP clients.
 *
 * This function sends the given message to all TCP clients currently connected
 * to the server. It iterates over the list of connected clients and sends the
 * message to each client.
 *
 * @param message The message to send to all connected TCP clients.
 */
void send_to_all_tcp_clients(const char* message);

/**
 * @brief Adds a UDP client to the list of connected clients.
 *
 * This function adds a UDP client represented by the provided UDPClientData structure
 * to the list of connected clients. If there is space available in the client list,
 * the client is added to the list.
 *
 * @param udp_clients A pointer to the UDPClientList structure representing the list of connected UDP clients.
 * @param client The UDPClientData structure representing the UDP client to be added.
 */
void add_udp_client(UDPClientList* udp_clients, UDPClientData client);

/**
 * @brief Removes a UDP client from the list of connected clients.
 *
 * This function removes a UDP client represented by the provided file descriptor
 * from the list of connected clients. It searches for the client in the list
 * and removes it if found. The function also shifts the remaining clients to fill
 * the gap left by the removed client.
 *
 * @param udp_clients A pointer to the UDPClientList structure representing the list of connected UDP clients.
 * @param sockfd The file descriptor of the UDP client to be removed.
 */
void remove_udp_client(UDPClientList* udp_clients, int sockfd);

/**
 * @brief Sends a message to all connected UDP clients.
 *
 * This function sends the provided message to all UDP clients currently connected
 * to the server. It iterates over the list of connected clients and sends the
 * message to each client using the sendto() system call.
 *
 * @param udp_clients A pointer to the UDPClientList structure representing the list of connected UDP clients.
 * @param message A pointer to the message to be sent.
 * @param message_len The length of the message to be sent.
 */
void send_to_all_udp_clients(UDPClientList* udp_clients, const char* message, size_t message_len);

/**
 * @brief Logs an event message to a log file.
 *
 * @param message The message to be logged.
 */
void log_event(const char* message);

/**
 * @brief Logs a JSON object as an event message to a log file.
 *
 * @param json The cJSON object to be logged as JSON string.
 */
void log_event_json(cJSON* json);

/**
 * @brief Retrieves the IP address of a TCP client.
 *
 * @param client_fd The file descriptor of the TCP client.
 * @param client_ip Pointer to a character array where the IP address will be stored.
 */
void get_tcp_client_ip(int client_fd, char* client_ip);

/**
 * @brief Accepts a connection on a Unix domain socket.
 *
 * This function accepts a connection on the specified Unix domain socket.
 *
 * @param sockfd The file descriptor of the Unix domain socket.
 * @param addr Pointer to a sockaddr structure where the address of the connecting client will be stored.
 * @param addrlen Pointer to a socklen_t variable that contains the size of the sockaddr structure. On return, it will
 * be modified to indicate the actual size of the address stored in addr.
 * @return On success, the file descriptor of the accepted connection is returned. On error, -1 is returned, and errno
 * is set appropriately.
 */
int accept_unix_connection(int sockfd, struct sockaddr* addr, socklen_t addrlen);

/**
 * @brief Creates a child process for handling infection alerts.
 *
 * It initializes necessary resources, such as sensors, and periodically checks for
 * temperature thresholds. If a threshold is exceeded, it writes to a FIFO file.
 * This function makes use of functions defined in the "alertInfection.h" library.
 *
 * @return void
 */
void create_infection_alerts_process();

/**
 * @brief Creates a child process for handling power outage notifications.
 *
 * It periodically sends failure messages to the parent process,
 * indicating a power outage. The time between messages is randomized between
 * 5 and 10 minutes. This function makes use of functions defined in the "emergNotif.h" library.
 *
 * @return void
 */
void create_power_outage_alert_process();

/**
 * @brief Initializes the count of alerts for each entry point.
 * @param ealerts The EntryAlertsCount structure to initialize.
 */
void initialize_entry_alerts_count(EntryAlertsCount* ealerts);

/**
 * @brief Retrieves the number of alerts for a specific entry point.
 *
 * @param entry The entry point for which to retrieve the alert count ("NORTH", "SOUTH", "EAST", or "WEST").
 * @return The number of alerts for the specified entry point, or -1 if the entry is invalid.
 */
int get_alerts_for_entry(const char* entry);

/**
 * @brief Creates a JSON summary containing information about alerts, supplies, and emergency events.
 *
 * @return A cJSON object representing the JSON summary.
 */
cJSON* create_summary_json();

/**
 * @brief Detects the entry point based on the alert message.
 *
 * @param alert_message The alert message to analyze.
 * @return A string representing the detected entry point ("NORTH", "SOUTH", "EAST", or "WEST"), or NULL if no valid
 * entry point is found in the message.
 */
const char* detect_entry(const char* alert_message);

/**
 * @brief Updates the emergency information with the latest keepalive timestamp and event.
 *
 * @param keepalived The timestamp of the last keepalive event.
 * @param event The description of the last event.
 */
void update_emergency_info(const char* keepalived, const char* event, EmergencyInfo* emergency_info);

/**
 * @brief Retrieves the timestamp of the last keepalive event.
 * @param emergency_info The emergency information structure.
 * @return The timestamp of the last keepalive event.
 */
const char* get_last_keepalived(EmergencyInfo* emergency_info);

/**
 * @brief Retrieves the description of the last event.
 * @param emergency_info The emergency information structure.
 * @return The description of the last event.
 */
const char* get_last_event(EmergencyInfo* emergency_info);

/**
 * @brief Get the home directory of the current user.
 *
 * This function retrieves the home directory of the user currently logged in.
 *
 * @return A pointer to a string containing the path to the home directory of the user.
 *         If the home directory cannot be determined, NULL is returned.
 */
const char* get_home_dir();
