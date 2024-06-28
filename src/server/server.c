#include "server.h"

/*Server status flag*/
volatile sig_atomic_t SERVER_RUNNING = 1; /* flag to control the server */

/*Global variables for child processes*/
pid_t alerts_pid;
pid_t check_alerts_pid;
pid_t power_outage_pid;

/*Global variables for shared memory*/
int shmid = -1;

int* alert_message;

/*Global variables for stuctures*/
TCPClientList tcp_clients;
UDPClientList udp_clients;
EntryAlertsCount entry_alerts_count;
EmergencyInfo emergency_info;

void start_server(int tcp_port, int udp_port)
{
    log_event("Server started");

    initialize_entry_alerts_count(&entry_alerts_count);

    struct sockaddr_in6 address_ipv6_tcp;
    struct sockaddr_in6 address_ipv6_udp;
    struct sockaddr_un address_unix;

    struct sockaddr_storage client_addr;
    socklen_t addrlen = sizeof(client_addr);

    memset(&address_ipv6_tcp, 0, sizeof(address_ipv6_tcp));
    memset(&address_ipv6_udp, 0, sizeof(address_ipv6_udp));
    memset(&address_unix, 0, sizeof(address_unix));

    const char* UNIX_SOCK_PATH = SOCK_PATH;

    // Clean up Unix domain socket if it already exists
    cleanup_unix_socket(UNIX_SOCK_PATH);

    printf("TCP Port selected by OS: %d\n", tcp_port);
    printf("UDP Port selected by OS: %d\n", udp_port);

    share_ports_shared_memory(tcp_port, udp_port);
    printf("Ports written in shared memory....\n");

    // Set up the TCP and UDP server sockets
    int tcp_socket_fd = set_tcp_socket(address_ipv6_tcp, tcp_port, MAX_CONNECTIONS);
    int udp_socket_fd = set_udp_socket(address_ipv6_udp, udp_port);
    int unix_socket_fd = set_unix_socket(UNIX_SOCK_PATH, 1, 1); // 1 for connection-oriented unix socket

    sleep(1); // wait to make sure that child process created the fifo

    int fifo_fd = open(FIFO_PATH, O_RDWR); // O_RDWR O_RDONLY
    if (fifo_fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // init shared memory with the supplies data module
    init_shared_memory_supplies();

    // File descriptor set for select
    fd_set master_fds;
    fd_set read_fds;
    int max_fd = tcp_socket_fd > udp_socket_fd ? tcp_socket_fd : udp_socket_fd;
    max_fd = max_fd > fifo_fd ? max_fd : fifo_fd;
    max_fd = max_fd > unix_socket_fd ? max_fd : unix_socket_fd;

    FD_ZERO(&master_fds);
    FD_SET(tcp_socket_fd, &master_fds);
    FD_SET(udp_socket_fd, &master_fds);
    FD_SET(unix_socket_fd, &master_fds);
    FD_SET(fifo_fd, &master_fds);

    printf("\U0001F4CB Logs available at: %s%s%s\n", get_home_dir(), LOG_DIR, LOG_FILENAME);
    printf("################################################\n");
    printf("############## Events - Messages ###############\n");
    printf("################################################\n");

    while (SERVER_RUNNING)
    {
        read_fds = master_fds;

        // Esperar por eventos de lectura en los sockets TCP y UDP
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Handle activity on the TCP and UDP sockets
        for (int fd = tcp_socket_fd; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &read_fds))
            {
                if (fd == tcp_socket_fd)
                {
                    int new_tcp_client_fd =
                        accept_tcp_connection(tcp_socket_fd, (struct sockaddr*)&client_addr, addrlen);
                    if (new_tcp_client_fd != -1)
                    {
                        FD_SET(new_tcp_client_fd, &master_fds);
                        if (new_tcp_client_fd > max_fd)
                        {
                            max_fd = new_tcp_client_fd;
                        }
                    }
                }
                else if (fd == udp_socket_fd)
                {
                    handle_udp_socket_activity(udp_socket_fd);
                }
                else if (fd == unix_socket_fd)
                {
                    handle_unix_socket_activity(unix_socket_fd, "Unix", 1);
                }
                else if (fd == fifo_fd)
                {
                    check_alerts();
                }
                else
                {
                    // printf("Connected TCP client activity\n");
                    handle_tcp_socket_activity(fd, &master_fds);
                }
            }
        }
    }
    log_event("Server turned off");
}

void handle_tcp_socket_activity(int client_fd, fd_set* master_fds)
{
    // Obtener la dirección IP del cliente
    struct sockaddr_storage client_addr;
    socklen_t addrlen = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr*)&client_addr, &addrlen) == 0)
    {
        char client_ip[INET6_ADDRSTRLEN]; // Use INET6_ADDRSTRLEN to accommodate IPv6 addresses
        if (client_addr.ss_family == AF_INET)
        {
            struct sockaddr_in* addr_ipv4 = (struct sockaddr_in*)&client_addr;
            inet_ntop(AF_INET, &(addr_ipv4->sin_addr), client_ip, INET6_ADDRSTRLEN);
        }
        else if (client_addr.ss_family == AF_INET6)
        {
            struct sockaddr_in6* addr_ipv6 = (struct sockaddr_in6*)&client_addr;
            inet_ntop(AF_INET6, &(addr_ipv6->sin6_addr), client_ip, INET6_ADDRSTRLEN);
        }
        else
        {
            strcpy(client_ip, "Unknown");
        }
        if (!check_tcp_clients_messages(client_fd))
        {
            // Print white circle
            printf("\033[37m\u25CF ");
            printf("\033[0m");
            printf("Error or disconnection occurred with TCP client at IP: %s\n", client_ip);
            char log_message[BUFFER_256]; // Allocate space for the log message
            snprintf(log_message, sizeof(log_message), "TCP client disconnected from IP: %s", client_ip);
            log_event(log_message);
            close(client_fd);
            FD_CLR(client_fd, master_fds);
            remove_tcp_client(client_fd, &tcp_clients);
        }
    }
    else
    {
        perror("getpeername");
        close(client_fd);
        FD_CLR(client_fd, master_fds);
        remove_tcp_client(client_fd, &tcp_clients); // Remove the client from the list of connected clients
    }
}

void handle_udp_socket_activity(int sockfd)
{
    // Manejar actividad en el socket UDP
    if (!check_udp_clients_messages(sockfd))
    {
        // Error o desconexión del cliente UDP
        printf("Error or disconnection occurred with UDP client.\n");
    }
}

void handle_unix_socket_activity(int sockfd, const char* client_type, int connection_oriented)
{
    if (connection_oriented)
    {
        int client_fd = accept_unix_connection(sockfd, NULL, 0);
        if (client_fd != -1)
        {
            // Handle the connection
            char buffer[BUFFER_SIZE];
            ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0'; // Add null terminator
                printf("Message received from %s client: %s\n", client_type, buffer);

                cJSON* disconnect_json = cJSON_CreateObject();
                cJSON_AddStringToObject(disconnect_json, "message", "disconnect");
                char* disconnect = cJSON_Print(disconnect_json);
                send_to_all_tcp_clients(disconnect);
                log_event(buffer);
                cJSON_Delete(disconnect_json);
            }
            else if (bytes_received == 0)
            {
                // printf("UNIX - No data received from %s client\n", client_type);
            }
            else
            {
                perror("recv");
            }
            close(client_fd); // Close the client socket
        }
    }
    else
    {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0'; // Add null terminator
            printf("Message received from %s client: %s\n", client_type, buffer);
        }
        else if (bytes_received == 0)
        {
            // printf("No data received from %s client\n", client_type);
        }
        else
        {
            perror("recv");
        }
    }
}

void sigint_handler()
{
    printf("\nServer shutting down...\n");
    SERVER_RUNNING = 0;

    if (alerts_pid > 0)
    {
        kill(alerts_pid, SIGINT);
    }
    if (power_outage_pid > 0)
    {
        kill(power_outage_pid, SIGINT);
    }
}

void cleanup_unix_socket(const char* socket_path)
{
    if (unlink(socket_path) < 0 && errno != ENOENT)
    {
        perror("Error while unlinking Unix domain socket");
    }
}

int get_selected_port(int sockfd)
{
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    if (getsockname(sockfd, (struct sockaddr*)&address, &addrlen) == -1)
    {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    return ntohs(address.sin_port);
}

int get_available_port()
{
    // Create an auxiliar socket to get the port
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(0); // Let the OS assign a port
    if (bind(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) == -1)
    {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Get the assigned port
    int assigned_port = get_selected_port(sockfd);

    // Close the socket
    close(sockfd);

    // Return the assigned port
    return assigned_port;
}

void share_ports_shared_memory(int tcp_port, int udp_port)
{
    // Define the structure to store TCP and UDP ports locally within the function
    struct PortInfo
    {
        int tcp_port;
        int udp_port;
    };

    // Create or open the shared memory region
    int shm_fd = shm_open(SHARED_MEM_PORTS, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        return;
    }

    // Truncate the shared memory region to the size of the PortInfo structure
    if (ftruncate(shm_fd, sizeof(struct PortInfo)) == -1)
    {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHARED_MEM_PORTS);
        return;
    }

    // Map the shared memory region into the process address space
    struct PortInfo* shared_port_info =
        mmap(NULL, sizeof(struct PortInfo), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_port_info == MAP_FAILED)
    {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHARED_MEM_PORTS);
        return;
    }

    // Write the TCP and UDP ports into the shared memory region
    shared_port_info->tcp_port = tcp_port;
    shared_port_info->udp_port = udp_port;

    // Unmap and close the shared memory region
    if (munmap(shared_port_info, sizeof(struct PortInfo)) == -1)
    {
        perror("munmap");
    }
    close(shm_fd);
}

void redirect_output_to_parent()
{
    if (dup2(STDOUT_FILENO, alerts_pid) == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    if (dup2(STDOUT_FILENO, power_outage_pid) == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
}

int accept_tcp_connection(int sockfd, struct sockaddr* addr, socklen_t addrlen)
{
    int client_fd = accept(sockfd, addr, &addrlen);
    if (client_fd == -1)
    {
        printf("Error accepting connection\n");
        perror("accept");
    }
    else
    {
        char client_ip[INET6_ADDRSTRLEN]; // Use INET6_ADDRSTRLEN to accommodate IPv6 addresses
        char log_message[BUFFER_256];     // Allocate space for the log message
        if (addr->sa_family == AF_INET6)
        {
            struct sockaddr_in6* client_addr_ipv6 = (struct sockaddr_in6*)addr;
            struct in6_addr ipv6_addr = client_addr_ipv6->sin6_addr;
            if (IN6_IS_ADDR_V4MAPPED(&ipv6_addr))
            {
                // IPv4-mapped IPv6 address detected
                struct in_addr ipv4_addr;
                memcpy(&ipv4_addr, &ipv6_addr.s6_addr[12], sizeof(struct in_addr));
                inet_ntop(AF_INET, &ipv4_addr, client_ip, INET_ADDRSTRLEN);
                printf("\033[32m\u25CF "); // Change color to green and then print filled circle
                printf("\033[0m");         // Restore color to default value
                printf("New TCP IPv4 client connected from IP: %s\n", client_ip);
                snprintf(log_message, sizeof(log_message), "New TCP IPv4 client connected from IP: %s", client_ip);
            }
            else
            {
                // Regular IPv6 address
                inet_ntop(AF_INET6, &client_addr_ipv6->sin6_addr, client_ip, INET6_ADDRSTRLEN);
                printf("\033[32m\u25CF "); // Change color to green and then print filled circle
                printf("\033[0m");         // Restore color to default value
                printf("New TCP IPv6 client connected from IP: %s\n", client_ip);
                snprintf(log_message, sizeof(log_message), "New TCP IPv6 client connected from IP: %s", client_ip);
            }
        }
        else if (addr->sa_family == AF_INET)
        {
            struct sockaddr_in* client_addr_ipv4 = (struct sockaddr_in*)addr;
            inet_ntop(AF_INET, &client_addr_ipv4->sin_addr, client_ip, INET_ADDRSTRLEN);
            snprintf(log_message, sizeof(log_message), "New IPv4 client connected from IP: %s", client_ip);
        }
        log_event(log_message);
        add_tcp_client(client_fd, &tcp_clients); // add the client to the list of connected clients
    }
    return client_fd;
}

int accept_unix_connection(int sockfd, struct sockaddr* addr, socklen_t addrlen)
{
    int client_fd = accept(sockfd, addr, &addrlen);
    if (client_fd == -1)
    {
        perror("accept");
    }
    return client_fd;
}

cJSON* receive_tcp_json(int sockfd)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0'; // add null terminator

        cJSON* json = cJSON_Parse(buffer);
        if (!json)
        {
            fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
            return NULL;
        }

        printf("JSON received from client: %s\n", buffer);
        return json;
    }
    else if (bytes_received == 0)
    {
        // printf("No data received from client\n");
    }
    else
    {
        perror("recv");
    }

    return NULL;
}

void send_json_to_tcp_client(int sockfd, cJSON* json)
{
    char* json_string = cJSON_Print(json);
    ssize_t bytes_sent = send(sockfd, json_string, strlen(json_string), 0);

    if (bytes_sent < 0)
    {
        perror("Error sending JSON to client");
    }
    else if (bytes_sent == 0)
    {
        printf("No data sent to client\n");
    }
    else
    {
        printf("JSON sent to client: %s\n", json_string);
    }

    free(json_string);
}

int check_tcp_clients_messages(int client_fd)
{
    // Receive JSON message from client
    cJSON* received_json = receive_tcp_json(client_fd);
    if (received_json)
    {
        // Access the 'message' field in the JSON object
        cJSON* message = cJSON_GetObjectItem(received_json, "message");
        if (message)
        {
            // Verify type of message
            const char* message_value = message->valuestring;
            if (strcmp(message_value, "authenticateme") == 0)
            {
                // Authentication message
                cJSON* hostname = cJSON_GetObjectItem(received_json, "hostname");
                if (hostname)
                {
                    const char* hostname_value = hostname->valuestring;
                    // Verify if the hostname is the same as the admin user
                    if (strcmp(hostname_value, ADMIN_USER) == 0)
                    {
                        char client_ip[INET6_ADDRSTRLEN];
                        get_tcp_client_ip(client_fd, client_ip);
                        char log_message[BUFFER_256];
                        snprintf(log_message, sizeof(log_message), "Update request from authenticated TCP client %s",
                                 client_ip);
                        log_event(log_message);
                        printf("Client TCP authenticated successfully.\n");
                        // Send authentication confirmation to client
                        cJSON* auth_confirmation = cJSON_CreateObject();
                        cJSON_AddStringToObject(auth_confirmation, "message", "auth_success");
                        send_json_to_tcp_client(client_fd, auth_confirmation);
                        cJSON_Delete(auth_confirmation);
                        cJSON_Delete(received_json);
                        return 1; // Successful authentication
                    }
                    else
                    {
                        char client_ip[INET6_ADDRSTRLEN];
                        get_tcp_client_ip(client_fd, client_ip);
                        char log_message[BUFFER_256];
                        snprintf(log_message, sizeof(log_message),
                                 "Update request from not authenticated TCP client %s", client_ip);
                        log_event(log_message);
                        printf("Client TCP authentication failed: Invalid hostname.\n");
                        cJSON* auth_failure = cJSON_CreateObject();
                        cJSON_AddStringToObject(auth_failure, "message", "auth_failure");
                        send_json_to_tcp_client(client_fd, auth_failure);
                        cJSON_Delete(auth_failure);
                        cJSON_Delete(received_json);
                        return 0; // Failed authentication
                    }
                }
            }
            else
            {
                FoodSupply* food_supply = get_food_supply();
                MedicineSupply* medicine_supply = get_medicine_supply();
                if (strcmp(message_value, "status") == 0)
                {
                    char client_ip[INET6_ADDRSTRLEN];
                    get_tcp_client_ip(client_fd, client_ip);
                    char log_message[BUFFER_256];

                    time_t rawtime;
                    struct tm* timeinfo;
                    time(&rawtime);
                    timeinfo = localtime(&rawtime);
                    char timestamp[20];
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

                    snprintf(log_message, sizeof(log_message), "Status request from TCP client %s", client_ip);
                    update_emergency_info(timestamp, log_message, &emergency_info);
                    log_event(log_message);
                    printf("Received request from client TCP: Status\n");
                    send_json_to_tcp_client(client_fd, convert_supplies_to_json(food_supply, medicine_supply));
                }
                else if (strcmp(message_value, "update") == 0)
                {
                    char client_ip[INET6_ADDRSTRLEN];
                    get_tcp_client_ip(client_fd, client_ip);
                    char log_message[BUFFER_256];

                    snprintf(log_message, sizeof(log_message), "Update request from TCP client %s", client_ip);

                    time_t rawtime;
                    struct tm* timeinfo;
                    time(&rawtime);
                    timeinfo = localtime(&rawtime);
                    char timestamp[20];
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
                    update_emergency_info(timestamp, log_message, &emergency_info);

                    log_event(log_message);
                    printf("Received request from client TCP: Update\n");

                    // Verify valid pointers to supplies data
                    if (food_supply != NULL && medicine_supply != NULL)
                    {
                        update_supplies_from_json(food_supply, medicine_supply, received_json);
                    }
                    else
                    {
                        printf("Error obtaining pointers to supplies data.\n");
                    }
                }
                else if (strcmp(message_value, "summary") == 0)
                {
                    char client_ip[INET6_ADDRSTRLEN];
                    get_tcp_client_ip(client_fd, client_ip);
                    char log_message[BUFFER_256];
                    snprintf(log_message, sizeof(log_message), "Summary request from TCP client %s", client_ip);
                    log_event(log_message); // Registrar evento
                    printf("Received request from client TCP: Summary\n");

                    cJSON* summary = create_summary_json();
                    send_json_to_tcp_client(client_fd, summary);
                }
                else
                {
                    char client_ip[INET6_ADDRSTRLEN];
                    get_tcp_client_ip(client_fd, client_ip);
                    char log_message[BUFFER_256];
                    snprintf(log_message, sizeof(log_message), "Invalid request received from TCP client %s",
                             client_ip);
                    log_event(log_message); // Registrar evento
                    printf("Invalid request received from client TCP\n");
                }
            }
        }
        cJSON_Delete(received_json);
        return 1; // Lectura exitosa
    }
    else
    {
        return 0; // Error o desconexión
    }
}

void handle_new_tcp_connection(int tcp_socket_fd)
{
    struct sockaddr_storage client_addr;
    socklen_t addrlen = sizeof(client_addr);
    accept_tcp_connection(tcp_socket_fd, (struct sockaddr*)&client_addr, addrlen);
}

void send_json_to_udp_client(int sockfd, struct sockaddr* client_addr, socklen_t client_addrlen, cJSON* json_response)
{
    // Convert cJSON object to JSON string
    char* json_string = cJSON_Print(json_response);

    // Send response back to client
    ssize_t bytes_sent = sendto(sockfd, json_string, strlen(json_string), 0, client_addr, client_addrlen);
    if (bytes_sent == -1)
    {
        perror("sendto");
        free(json_string);
        return;
    }

    // Print message sent information
    char client_ip[INET6_ADDRSTRLEN];
    int client_port;

    get_udp_client_info((struct sockaddr_storage*)client_addr, client_ip, sizeof(client_ip), &client_port);

    printf("Message sent to %s:%d\n", client_ip, client_port);

    // Clean up cJSON object and JSON string
    cJSON_Delete(json_response);
    free(json_string);
}

void get_udp_client_info(struct sockaddr_storage* client_addr, char* client_ip, size_t ip_buffer_size, int* client_port)
{
    if (client_addr->ss_family == AF_INET)
    {
        struct sockaddr_in* ipv4_addr = (struct sockaddr_in*)client_addr;
        inet_ntop(AF_INET, &ipv4_addr->sin_addr, client_ip, (socklen_t)ip_buffer_size);
        *client_port = ntohs(ipv4_addr->sin_port);
    }
    else if (client_addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6* ipv6_addr = (struct sockaddr_in6*)client_addr;
        inet_ntop(AF_INET6, &ipv6_addr->sin6_addr, client_ip, (socklen_t)ip_buffer_size);
        *client_port = ntohs(ipv6_addr->sin6_port);
    }
    else
    {
        printf("Unknown address family\n");
        *client_port = -1;
    }
}

cJSON* receive_udp_json(int sockfd, struct sockaddr_storage* client_addr, socklen_t* client_addrlen)
{
    char buffer[BUFFER_SIZE];

    // Receive message from client
    ssize_t bytes_received =
        recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)client_addr, client_addrlen);
    if (bytes_received == -1)
    {
        perror("recvfrom");
        return NULL;
    }

    // Null-terminate the received data
    buffer[bytes_received] = '\0';

    // Get client information
    char client_ip[INET6_ADDRSTRLEN];
    int client_port;
    get_udp_client_info(client_addr, client_ip, sizeof(client_ip), &client_port);

    // Print the received message and client information
    printf("Received UDP message from %s:%d: %s\n", client_ip, client_port, buffer);

    // Parse the received JSON string
    cJSON* json = cJSON_Parse(buffer);
    if (!json)
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }

    return json;
}

int check_udp_clients_messages(int sockfd)
{
    // Receive JSON message from client
    struct sockaddr_storage client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    cJSON* received_json = receive_udp_json(sockfd, &client_addr, &client_addrlen);
    if (!received_json)
    {
        // Error occurred while receiving JSON or parsing it
        return 0; // Error or disconnection
    }

    // Get client information
    char client_ip[INET6_ADDRSTRLEN];
    int client_port;
    get_udp_client_info(&client_addr, client_ip, sizeof(client_ip), &client_port);

    // Add the new UDP client to the list of connected clients
    UDPClientData new_client;
    new_client.sockfd = sockfd;
    new_client.client_addr = client_addr;
    new_client.addr_len = client_addrlen;
    add_udp_client(&udp_clients, new_client);

    // Check if data has the 'message' field
    cJSON* message = cJSON_GetObjectItem(received_json, "message");
    if (!message)
    {
        printf("No 'message' field found in JSON\n");
        cJSON_Delete(received_json);
        return 0; // Error
    }
    // Check if data has the 'hostname' field
    cJSON* hostname = cJSON_GetObjectItem(received_json, "hostname");
    if (!hostname)
    {
        printf("No 'hostname' field found in JSON\n");
        cJSON_Delete(received_json);
        return 0; // Error
    }

    // Check the value of the 'message' and 'hostname' fields
    const char* value = message->valuestring;
    const char* auth = hostname->valuestring;

    FoodSupply* food_supply = get_food_supply();
    MedicineSupply* medicine_supply = get_medicine_supply();

    if (strcmp(value, "update") == 0)
    {
        printf("Received request from UDP client: Update\n");
        if (strcmp(auth, ADMIN_USER) == 0)
        {
            printf("Client successfully authenticated\n");
            update_supplies_from_json(food_supply, medicine_supply, received_json);
            // Log event for update request from authenticated client
            char log_message[BUFFER_256];
            snprintf(log_message, sizeof(log_message), "Update request from authenticated UDP client %s", client_ip);
            log_event(log_message);
        }
        else
        {
            printf("Not authenticated client tried to update data\n");
            // Log event for update request from non-authenticated client
            char log_message[BUFFER_256];
            snprintf(log_message, sizeof(log_message), "Update request from not authenticated UDP client %s",
                     client_ip);
            log_event(log_message);
        }
    }
    else if (strcmp(value, "status") == 0)
    {
        printf("Received request from UDP client: Status\n");
        // Log event for status request from UDP client
        char log_message[BUFFER_256];
        snprintf(log_message, sizeof(log_message), "Status request from UDP client %s", client_ip);
        log_event(log_message);
        send_json_to_udp_client(sockfd, (struct sockaddr*)&client_addr, client_addrlen,
                                convert_supplies_to_json(food_supply, medicine_supply));
    }
    else if (strcmp(value, "summary") == 0)
    {
        printf("Received request from UDP client: Summary\n");
        char log_message[BUFFER_256];
        snprintf(log_message, sizeof(log_message), "Summary request from UDP client %s", client_ip);
        log_event(log_message);
        cJSON* summary = create_summary_json();
        send_json_to_udp_client(sockfd, (struct sockaddr*)&client_addr, client_addrlen, summary);
    }
    else
    {
        printf("Invalid request received from UDP client\n");
    }
    cJSON_Delete(received_json);
    return 1; // Successful read
}

void parse_command_line_arguments(int argc, char* argv[], int* tcp_port, int* udp_port)
{
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            if (strcmp(optarg, "tcp") == 0)
            {
                if (optind < argc)
                {
                    *tcp_port = atoi(argv[optind]);
                }
                else
                {
                    printf("TCP port must be specified.\n");
                    exit(EXIT_FAILURE);
                }
            }
            else if (strcmp(optarg, "udp") == 0)
            {
                if (optind < argc)
                {
                    *udp_port = atoi(argv[optind]);
                }
                else
                {
                    printf("UDP port must be specified.\n");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                printf("Invalid -p option. It should be 'tcp' or 'udp'.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            printf("Usage: %s -p tcp <tcp_port> -p udp <udp_port>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

void set_ports(int* tcp_port, int* udp_port)
{
    // Check if only one port is specified
    if (*tcp_port == -1 && *udp_port != -1)
    {
        // Use default TCP port
        *tcp_port = get_available_port();
    }
    else if (*tcp_port != -1 && *udp_port == -1)
    {
        // Use default UDP port
        *udp_port = get_available_port();
    }

    // Start the server with the specified ports (or default values if not provided)
    if (*tcp_port == -1)
    {
        *tcp_port = get_available_port();
    }
    if (*udp_port == -1)
    {
        *udp_port = get_available_port();
    }
}

cJSON* convert_supplies_to_json(FoodSupply* food_supply, MedicineSupply* medicine_supply)
{
    // Create a JSON object to hold the supplies data
    cJSON* supplies_json = cJSON_CreateObject();
    if (supplies_json == NULL)
    {
        printf("Error creating JSON object for supplies.\n");
        return NULL;
    }

    // Create a JSON object for food supplies
    cJSON* food_json = cJSON_CreateObject();
    if (food_json == NULL)
    {
        printf("Error creating JSON object for food supplies.\n");
        cJSON_Delete(supplies_json);
        return NULL;
    }

    // Add food supplies data to the JSON object
    cJSON_AddItemToObject(supplies_json, "food", food_json);
    cJSON_AddNumberToObject(food_json, "meat", food_supply->meat);
    cJSON_AddNumberToObject(food_json, "vegetables", food_supply->vegetables);
    cJSON_AddNumberToObject(food_json, "fruits", food_supply->fruits);
    cJSON_AddNumberToObject(food_json, "water", food_supply->water);

    // Create a JSON object for medicine supplies
    cJSON* medicine_json = cJSON_CreateObject();
    if (medicine_json == NULL)
    {
        printf("Error creating JSON object for medicine supplies.\n");
        cJSON_Delete(supplies_json);
        return NULL;
    }

    // Add medicine supplies data to the JSON object
    cJSON_AddItemToObject(supplies_json, "medicine", medicine_json);
    cJSON_AddNumberToObject(medicine_json, "antibiotics", medicine_supply->antibiotics);
    cJSON_AddNumberToObject(medicine_json, "analgesics", medicine_supply->analgesics);
    cJSON_AddNumberToObject(medicine_json, "bandages", medicine_supply->bandages);

    return supplies_json;
}

Sensor* initiateAlertModule()
{

    if (mkfifo(FIFO_PATH, 0666) == -1)
    {
        perror("Error creating FIFO");
    }

    Sensor* sensors = malloc(sizeof(Sensor) * NUM_SENSORS);
    if (sensors == NULL)
    {
        perror("Error allocating memory for sensors");
        exit(EXIT_FAILURE);
    }

    sprintf(sensors[0].name, "NORTH ENTRY");
    sprintf(sensors[1].name, "SOUTH ENTRY");
    sprintf(sensors[2].name, "WEST ENTRY");
    sprintf(sensors[3].name, "EAST ENTRY");

    return sensors;
}

void check_alerts()
{
    int fd;
    char alert_message[BUFFER_SIZE];

    memset(alert_message, 0, sizeof(alert_message)); // Fill alert_message with 0

    fd = open(FIFO_PATH, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening FIFO");
        return;
    }

    // Read any available alerts from the FIFO
    ssize_t bytes_read = read(fd, alert_message, sizeof(alert_message));
    if (bytes_read > 0)
    {
        log_event(alert_message);

        time_t rawtime;
        struct tm* timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        update_emergency_info(timestamp, alert_message, &emergency_info);

        send_to_all_tcp_clients(alert_message);
        send_to_all_udp_clients(&udp_clients, alert_message, strlen(alert_message));
        log_event("Sent alert notification to all connected clients");

        const char* entry = detect_entry(alert_message);
        if (entry != NULL)
        {
            // Found a valid entry in the alert message
            printf("\u26A0 Detected alert at entry: %s\n", entry);
            printf("\U0001F4E2 Sent alert notification to all connected clients\n");

            // Increase corresponding entry count
            if (strcmp(entry, "NORTH") == 0)
            {
                entry_alerts_count.north++;
            }
            else if (strcmp(entry, "SOUTH") == 0)
            {
                entry_alerts_count.south++;
            }
            else if (strcmp(entry, "EAST") == 0)
            {
                entry_alerts_count.east++;
            }
            else if (strcmp(entry, "WEST") == 0)
            {
                entry_alerts_count.west++;
            }
        }
    }
    else if (bytes_read == 0)
    {
        // No data available
        // printf("No data available\n");
    }
    else
    {
        perror("Error reading from FIFO");
    }
    close(fd);
}

void cleanup_fifo(const char* fifo_path)
{
    // Remove the FIFO file
    if (unlink(fifo_path) == -1 && errno != ENOENT)
    {
        perror("Error while unlinking FIFO");
    }
}

int fifo_not_empty(int fd)
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    // Configure timeout to return immediately
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Call select to check if the FIFO is ready for reading
    int ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready == -1)
    {
        perror("Error en select");
        exit(EXIT_FAILURE);
    }

    // return 1 if the FIFO is ready for reading
    return FD_ISSET(fd, &read_fds);
}

void add_tcp_client(int client_fd, TCPClientList* tcp_clients_list)
{
    if (tcp_clients_list->num_clients < MAX_CLIENTS)
    {
        tcp_clients_list->client_fds[tcp_clients_list->num_clients++] = client_fd;
        char num_clients_str[10]; // Space to hold the number of clients as text
        snprintf(num_clients_str, sizeof(num_clients_str), "%d", tcp_clients_list->num_clients);
        char log_message[BUFFER_256]; // space to store the log message
        snprintf(log_message, sizeof(log_message), "Added TCP client. Total connected: %s", num_clients_str);
        log_event(log_message);
    }
    else
    {
        printf("Can't add more clients\n");
    }
}

void remove_tcp_client(int client_fd, TCPClientList* tcp_clients)
{
    int i;
    for (i = 0; i < tcp_clients->num_clients; ++i)
    {
        if (tcp_clients->client_fds[i] == client_fd)
        {
            // Move the rest of the clients one position to the left
            for (; i < tcp_clients->num_clients - 1; ++i)
            {
                tcp_clients->client_fds[i] = tcp_clients->client_fds[i + 1];
            }
            tcp_clients->num_clients--;
            char num_clients_str[10]; // Space to store the number as a string
            snprintf(num_clients_str, sizeof(num_clients_str), "%d", tcp_clients->num_clients);
            char log_message[BUFFER_256]; // Space for the log message
            snprintf(log_message, sizeof(log_message), "TCP client disconnected. Total connected: %s", num_clients_str);
            log_event(log_message);
            return;
        }
    }
}

void send_to_all_tcp_clients(const char* message)
{
    for (int i = 0; i < tcp_clients.num_clients; i++)
    {
        send(tcp_clients.client_fds[i], message, strlen(message), 0);
    }
}

void add_udp_client(UDPClientList* udp_clients, UDPClientData client)
{
    // Check if the client already exists in the list
    for (int i = 0; i < udp_clients->num_clients; ++i)
    {
        struct sockaddr_in* existing_addr = (struct sockaddr_in*)&udp_clients->clients[i].client_addr;
        struct sockaddr_in* new_addr = (struct sockaddr_in*)&client.client_addr;

        // Compare client address and port
        if (existing_addr->sin_addr.s_addr == new_addr->sin_addr.s_addr &&
            existing_addr->sin_port == new_addr->sin_port)
        {
            // Client already exists, do not add
            printf("Client already exists in the UDP client list.\n");
            return;
        }
    }

    // Check if the maximum number of clients has been reached
    if (udp_clients->num_clients < MAX_CLIENTS)
    {
        // Add the new client to the list
        udp_clients->clients[udp_clients->num_clients++] = client;

        // Generate log event
        char num_clients_str[10]; // String to store the number of clients as text
        snprintf(num_clients_str, sizeof(num_clients_str), "%d", udp_clients->num_clients);
        char log_message[BUFFER_256];
        snprintf(log_message, sizeof(log_message), "Added UDP client. Total cached: %s", num_clients_str);
        log_event(log_message); // Register the event
    }
    else
    {
        printf("Maximum number of clients reached. Cannot add more clients.\n");
    }
}

void remove_udp_client(UDPClientList* udp_clients, int sockfd)
{
    for (int i = 0; i < udp_clients->num_clients; ++i)
    {
        if (udp_clients->clients[i].sockfd == sockfd)
        {
            // Shift remaining clients to fill the gap
            for (int j = i; j < udp_clients->num_clients - 1; ++j)
            {
                udp_clients->clients[j] = udp_clients->clients[j + 1];
            }
            udp_clients->num_clients--;
            break;
        }
    }
}

void send_to_all_udp_clients(UDPClientList* udp_clients, const char* message, size_t message_len)
{
    for (int i = 0; i < udp_clients->num_clients; ++i)
    {
        sendto(udp_clients->clients[i].sockfd, message, message_len, 0,
               (struct sockaddr*)&(udp_clients->clients[i].client_addr), udp_clients->clients[i].addr_len);
    }
}

void log_event(const char* message)
{
    FILE* logFile;
    time_t currentTime;
    struct tm* localTime;
    char timestamp[BUFFER_256];
    char logDirPath[BUFFER_256];
    char logFilePath[BUFFER_521];

    // Get the user's home directory
    const char* homeDir = get_home_dir();

    // Construct log directory path
    snprintf(logDirPath, sizeof(logDirPath), "%s%s", homeDir, LOG_DIR);

    // Create ~/.refuge directory if it doesn't exist
    struct stat st = {0};
    if (stat(logDirPath, &st) == -1)
    {
        if (mkdir(logDirPath, 0700) == -1)
        {
            perror("Error creating directory");
            return;
        }
        printf("Directory created: %s\n", logDirPath);
    }

    // Construct log file path
    snprintf(logFilePath, sizeof(logFilePath), "%s%s", logDirPath, LOG_FILENAME);

    // Open the log file in append mode
    logFile = fopen(logFilePath, "a+");
    if (logFile == NULL)
    {
        perror("Error opening log file");
        return;
    }

    // Get current time
    currentTime = time(NULL);
    localTime = localtime(&currentTime);

    // Format date and time
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", localTime);

    // Write timestamp and message to the log file
    fprintf(logFile, "%s %s\n", timestamp, message);

    // Close the log file
    fclose(logFile);
}

void log_event_json(cJSON* json)
{
    char* jsonString = cJSON_Print(json);
    log_event(jsonString);
    free(jsonString); // Free the allocated memory for JSON string
}

void get_tcp_client_ip(int client_fd, char* client_ip)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    char ip[INET6_ADDRSTRLEN];

    if (getpeername(client_fd, (struct sockaddr*)&addr, &addr_len) == 0)
    {
        if (addr.ss_family == AF_INET)
        {
            struct sockaddr_in* s = (struct sockaddr_in*)&addr;
            inet_ntop(AF_INET, &s->sin_addr, ip, sizeof(ip));
        }
        else // AF_INET6
        {
            struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
            inet_ntop(AF_INET6, &s->sin6_addr, ip, sizeof(ip));
        }
    }
    else
    {
        // Error al obtener la dirección IP del cliente
        strcpy(client_ip, "Unknown");
        return;
    }

    strcpy(client_ip, ip);
}

void create_infection_alerts_process()
{
    // Create a child process for handling infection alerts
    alerts_pid = fork();
    if (alerts_pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (alerts_pid == 0)
    {
        cleanup_fifo(FIFO_PATH); // Remove the FIFO file if it already exists
        // printf("PID sensor updates: %d\n", getpid());
        Sensor* sensors = initiateAlertModule();

        while (SERVER_RUNNING)
        {
            sleep((unsigned int)30);
            update_sensor_values(sensors, NUM_SENSORS);
            check_temperature_threshold(sensors,
                                        NUM_SENSORS); // writes fifo if temperature in some sensor higher than 38
        }
        free(sensors);
        exit(EXIT_SUCCESS);
    }
}

void create_power_outage_alert_process()
{
    // Create a child process for handling power outage notifications
    pid_t power_outage_pid = fork();
    if (power_outage_pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (power_outage_pid == 0)
    {
        // Code for the emergency notification handling child process
        // printf("PID power outage simulator: %d\n", getpid());
        sleep(1); // Wait for the parent to create the socket
        srand((unsigned int)time(NULL));
        while (SERVER_RUNNING)
        {
            // sleep(20);
            sleep((unsigned int)(SECONDS_IN_MINUTE * get_random_failure_minutes())); // Sleep for the failure interval
            int fd = init_emergency_notification();
            simulate_electricity_failure(fd); // Send failure message to parent process
            close(fd);
        }

        exit(EXIT_SUCCESS);
    }
}

void initialize_entry_alerts_count(EntryAlertsCount* ealerts)
{
    ealerts->north = 0;
    ealerts->south = 0;
    ealerts->east = 0;
    ealerts->west = 0;
}

int get_alerts_for_entry(const char* entry)
{
    if (strcmp(entry, "NORTH") == 0)
    {
        return entry_alerts_count.north;
    }
    else if (strcmp(entry, "SOUTH") == 0)
    {
        return entry_alerts_count.south;
    }
    else if (strcmp(entry, "EAST") == 0)
    {
        return entry_alerts_count.east;
    }
    else if (strcmp(entry, "WEST") == 0)
    {
        return entry_alerts_count.west;
    }
    else
    {
        // invalid entry
        return -1;
    }
}

const char* detect_entry(const char* alert_message)
{
    if (strstr(alert_message, "NORTH") != NULL)
    {
        return "NORTH";
    }
    else if (strstr(alert_message, "SOUTH") != NULL)
    {
        return "SOUTH";
    }
    else if (strstr(alert_message, "EAST") != NULL)
    {
        return "EAST";
    }
    else if (strstr(alert_message, "WEST") != NULL)
    {
        return "WEST";
    }
    else
    {
        return NULL; // No se encontró ninguna entrada válida en el mensaje
    }
}

cJSON* create_summary_json()
{
    cJSON* summary = cJSON_CreateObject();

    cJSON* alerts = cJSON_AddObjectToObject(summary, "alerts");
    cJSON_AddNumberToObject(alerts, "north_entry", get_alerts_for_entry("NORTH"));
    cJSON_AddNumberToObject(alerts, "east_entry", get_alerts_for_entry("EAST"));
    cJSON_AddNumberToObject(alerts, "west_entry", get_alerts_for_entry("WEST"));
    cJSON_AddNumberToObject(alerts, "south_entry", get_alerts_for_entry("SOUTH"));

    FoodSupply* food_supply = get_food_supply();
    MedicineSupply* medicine_supply = get_medicine_supply();
    cJSON* supplies = convert_supplies_to_json(food_supply, medicine_supply);
    cJSON_AddItemToObject(summary, "supplies", supplies);

    // Add emergency information
    cJSON* emergency = cJSON_AddObjectToObject(summary, "emergency");
    cJSON_AddStringToObject(emergency, "last_keepalived", get_last_keepalived(&emergency_info));
    cJSON_AddStringToObject(emergency, "last_event", get_last_event(&emergency_info));

    return summary;
}

void update_emergency_info(const char* keepalived, const char* event, EmergencyInfo* emergency_info)
{
    strncpy(emergency_info->last_keepalived, keepalived, sizeof(emergency_info->last_keepalived));
    strncpy(emergency_info->last_event, event, sizeof(emergency_info->last_event));
}

const char* get_last_keepalived(EmergencyInfo* emergency_info)
{
    return emergency_info->last_keepalived;
}

const char* get_last_event(EmergencyInfo* emergency_info)
{
    return emergency_info->last_event;
}

const char* get_home_dir()
{
    const char* homeDir = getenv("HOME");
    if (homeDir == NULL)
    {
        perror("Error getting home directory");
        exit(EXIT_FAILURE);
    }
    return homeDir;
}
