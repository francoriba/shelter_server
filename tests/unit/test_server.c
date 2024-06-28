#include "../../include/server.h"
#include "../../include/server_mocks.h"
#include <unity.h>

void test_add_tcp_client(void)
{
    TCPClientList* tcp_clients = malloc(sizeof(TCPClientList)); // Allocate memory for the TCPClientList
    if (tcp_clients == NULL)
    {
        // Handle memory allocation failure
        TEST_FAIL_MESSAGE("Failed to allocate memory for tcp_clients");
        return;
    }

    tcp_clients->num_clients = 0;

    add_tcp_client(324, tcp_clients);
    add_tcp_client(234, tcp_clients);
    add_tcp_client(123, tcp_clients);

    TEST_ASSERT_EQUAL(3, tcp_clients->num_clients);

    free(tcp_clients); // Free allocated memory
}

void test_get_available_port(void)
{
    int port = get_available_port();
    TEST_ASSERT_TRUE(port > 0);
    TEST_ASSERT_TRUE(port < 65535);
}

void test_parse_command_line_arguments()
{
    int tcp_port = -1;
    int udp_port = -1;

    char* argv[] = {"program_name", "-p", "tcp", "8080", "-p", "udp", "9090"};
    int argc = sizeof(argv) / sizeof(argv[0]);

    parse_command_line_arguments(argc, argv, &tcp_port, &udp_port);

    TEST_ASSERT_EQUAL_INT(8080, tcp_port);
    TEST_ASSERT_EQUAL_INT(9090, udp_port);
}

void test_parse_command_line_arguments_invalid_option()
{
    int tcp_port = -1;
    int udp_port = -1;

    char* argv[] = {"program_name", "-x", "tcp", "8080"};
    int argc = sizeof(argv) / sizeof(argv[0]);

    parse_command_line_arguments(argc, argv, &tcp_port, &udp_port);
}

void test_set_ports_not_specified()
{
    int tcp_port = -1;
    int udp_port = -1;

    // call withouth specifying ports
    set_ports(&tcp_port, &udp_port);

    // check that OS assigned ports
    TEST_ASSERT_TRUE(tcp_port > 0);
    TEST_ASSERT_TRUE(tcp_port < 65535);
    TEST_ASSERT_TRUE(udp_port > 0);
    TEST_ASSERT_TRUE(udp_port < 65535);
}

void test_set_ports_tcp_specified()
{
    int tcp_port = 8080;
    int udp_port = -1;

    set_ports(&tcp_port, &udp_port);

    // Check the udp port was assigned by OS
    TEST_ASSERT_TRUE(udp_port > 0);
    TEST_ASSERT_TRUE(udp_port < 65535);
    // Check the tcp port was not changed
    TEST_ASSERT_EQUAL_INT(8080, tcp_port);
}

void test_set_ports_udp_specified()
{
    int tcp_port = -1;
    int udp_port = 8080;

    set_ports(&tcp_port, &udp_port);

    // Check the tcp port was assigned by OS
    TEST_ASSERT_TRUE(tcp_port > 0);
    TEST_ASSERT_TRUE(tcp_port < 65535);
    // Check the udp port was not changed
    TEST_ASSERT_EQUAL_INT(8080, udp_port);
}

void test_set_ports_both_specified()
{
    int tcp_port = 8080;
    int udp_port = 9090;

    set_ports(&tcp_port, &udp_port);

    TEST_ASSERT_EQUAL_INT(8080, tcp_port);
    TEST_ASSERT_EQUAL_INT(9090, udp_port);
}

void test_convert_supplies_to_json()
{
    // Mock the supplies
    FoodSupply food_supply = {10, 20, 30, 40};
    MedicineSupply medicine_supply = {50, 60, 70};

    // Call the function to convert the supplies to a JSON object
    cJSON* supplies_json = convert_supplies_to_json(&food_supply, &medicine_supply);

    // Check the JSON object is not NULL
    TEST_ASSERT_NOT_NULL(supplies_json);

    // Check the JSON object has the expected structure
    cJSON* food_json = cJSON_GetObjectItemCaseSensitive(supplies_json, "food");
    TEST_ASSERT_NOT_NULL(food_json);
    TEST_ASSERT_EQUAL_INT(10, cJSON_GetObjectItemCaseSensitive(food_json, "meat")->valueint);
    TEST_ASSERT_EQUAL_INT(20, cJSON_GetObjectItemCaseSensitive(food_json, "vegetables")->valueint);
    TEST_ASSERT_EQUAL_INT(30, cJSON_GetObjectItemCaseSensitive(food_json, "fruits")->valueint);
    TEST_ASSERT_EQUAL_INT(40, cJSON_GetObjectItemCaseSensitive(food_json, "water")->valueint);

    cJSON* medicine_json = cJSON_GetObjectItemCaseSensitive(supplies_json, "medicine");
    TEST_ASSERT_NOT_NULL(medicine_json);
    TEST_ASSERT_EQUAL_INT(50, cJSON_GetObjectItemCaseSensitive(medicine_json, "antibiotics")->valueint);
    TEST_ASSERT_EQUAL_INT(60, cJSON_GetObjectItemCaseSensitive(medicine_json, "analgesics")->valueint);
    TEST_ASSERT_EQUAL_INT(70, cJSON_GetObjectItemCaseSensitive(medicine_json, "bandages")->valueint);

    cJSON_Delete(supplies_json);
}

void test_initiateAlertModule()
{
    Sensor* sensors = initiateAlertModule();

    // Check if FIFO file was created
    TEST_ASSERT_TRUE(access(FIFO_PATH, F_OK) == 0);

    // Check if memory for sensors was allocated
    TEST_ASSERT_NOT_NULL(sensors);

    // Check sensor names
    TEST_ASSERT_EQUAL_STRING("NORTH ENTRY", sensors[0].name);
    TEST_ASSERT_EQUAL_STRING("SOUTH ENTRY", sensors[1].name);
    TEST_ASSERT_EQUAL_STRING("WEST ENTRY", sensors[2].name);
    TEST_ASSERT_EQUAL_STRING("EAST ENTRY", sensors[3].name);

    // Free the allocated memory
    free(sensors);
}

void test_cleanup_fifo()
{
    // Mock FIFO file path
    const char* fifo_path = "test_fifo";

    // Dummy FIFO file
    int fd = open(fifo_path, O_CREAT | O_WRONLY, 0666);
    close(fd);

    // Call the function under test
    cleanup_fifo(fifo_path);

    // Check if the FIFO file was successfully removed
    TEST_ASSERT_TRUE(access(fifo_path, F_OK) == -1);
}

void test_fifo_not_empty()
{
    const char* fifo_path = "/tmp/testFifo"; // Adjust the path as needed
    int fd = open(fifo_path, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        perror("Error opening FIFO");
        exit(EXIT_FAILURE);
    }

    // Write some data to the FIFO
    const char* data = "Test data";
    write(fd, data, strlen(data));

    // Close the FIFO
    close(fd);

    // Reopen the FIFO for reading
    fd = open(fifo_path, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening FIFO");
        exit(EXIT_FAILURE);
    }

    // Call the function under test
    int result = fifo_not_empty(fd);

    // Assert that the FIFO is not empty
    TEST_ASSERT_EQUAL_INT(1, result);

    // Close the FIFO
    close(fd);

    // Remove the FIFO
    unlink(fifo_path);
}

void test_remove_tcp_client()
{
    TCPClientList tcp_clients;
    tcp_clients.num_clients = 3;
    tcp_clients.client_fds[0] = 1;
    tcp_clients.client_fds[1] = 2;
    tcp_clients.client_fds[2] = 3;

    // Remove one of the clients
    int client_fd_to_remove = 2;
    remove_tcp_client(client_fd_to_remove, &tcp_clients);

    // Check if the client was removed successfully
    TEST_ASSERT_EQUAL_INT(2, tcp_clients.num_clients);
    TEST_ASSERT_EQUAL_INT(1, tcp_clients.client_fds[0]);
    TEST_ASSERT_EQUAL_INT(3, tcp_clients.client_fds[1]);

    // Add additional assertions as needed
}

void test_add_udp_client()
{
    // Create an empty UDPClientList
    UDPClientList udp_clients;
    udp_clients.num_clients = 0;

    // Create a new UDP client to add
    UDPClientData new_client;
    memset(&new_client, 0, sizeof(new_client)); // Initialize with zeros

    // Set the address family to AF_INET
    new_client.client_addr.ss_family = AF_INET;

    // Cast sockaddr_storage to sockaddr_in since we're using AF_INET
    struct sockaddr_in* addr_in = (struct sockaddr_in*)&new_client.client_addr;

    addr_in->sin_port = htons(1234);                         // Example port number
    inet_pton(AF_INET, "192.168.1.100", &addr_in->sin_addr); // Example IP address

    // Call the function under test
    add_udp_client(&udp_clients, new_client);

    // Check if the client was added successfully
    TEST_ASSERT_EQUAL_INT(1, udp_clients.num_clients);

    // Cast the sockaddr_storage in the UDPClientList to sockaddr_in for comparison
    struct sockaddr_in* added_client_addr = (struct sockaddr_in*)&udp_clients.clients[0].client_addr;

    TEST_ASSERT_EQUAL_UINT(AF_INET, added_client_addr->sin_family);
    TEST_ASSERT_EQUAL_UINT(htons(1234), added_client_addr->sin_port);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", inet_ntoa(added_client_addr->sin_addr));
}

void test_remove_udp_client()
{
    UDPClientList udp_clients;
    udp_clients.num_clients = 3;
    UDPClientData client1, client2, client3;
    client1.sockfd = 1001;
    client2.sockfd = 1002;
    client3.sockfd = 1003;
    udp_clients.clients[0] = client1;
    udp_clients.clients[1] = client2;
    udp_clients.clients[2] = client3;

    int sockfd_to_remove = 1002;
    remove_udp_client(&udp_clients, sockfd_to_remove);

    TEST_ASSERT_EQUAL_INT(2, udp_clients.num_clients);
    TEST_ASSERT_EQUAL_INT(1001, udp_clients.clients[0].sockfd);
    TEST_ASSERT_EQUAL_INT(1003, udp_clients.clients[1].sockfd);
}

void test_logEvent()
{

    const char* homeDir = get_home_dir();
    char fullPath[256]; // Tamaño arbitrario para el buffer
    snprintf(fullPath, sizeof(fullPath), "%s%s%s", homeDir, LOG_DIR, LOG_FILENAME);

    // Define a test message
    const char* test_message = "Test message";

    // Call the function under test
    log_event(test_message);

    // Open the log file
    FILE* log_file = fopen(fullPath, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(log_file, "Failed to open log file");

    // Read the log file line by line
    char line[256];
    int message_found = 0;
    while (fgets(line, sizeof(line), log_file))
    {
        // Check if the message is present in the log file
        if (strstr(line, test_message) != NULL)
        {
            message_found = 1;
            break;
        }
    }

    // Close the log file
    fclose(log_file);

    // Assert that the message was found in the log file
    TEST_ASSERT_TRUE_MESSAGE(message_found, "Test message not found in log file");
}

void test_logEventJSON()
{

    const char* homeDir = get_home_dir();
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s%s", homeDir, LOG_DIR, LOG_FILENAME);

    // Create a cJSON object representing some JSON data
    cJSON* expected_json = cJSON_CreateObject();
    cJSON_AddStringToObject(expected_json, "key", "value");

    // Call the function under test
    log_event_json(expected_json);

    // Clean up cJSON object
    cJSON_Delete(expected_json);

    // Open the log file
    FILE* log_file = fopen(fullPath, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(log_file, "Failed to open log file");

    // Read the log file line by line
    char line[256];
    while (fgets(line, sizeof(line), log_file))
    {
        // Try to parse the line as JSON
        cJSON* json = cJSON_Parse(line);
        if (json != NULL)
        {
            // Compare the parsed JSON with the expected JSON
            if (cJSON_Compare(json, expected_json, 1))
            {
                cJSON_Delete(json);
                break;
            }
            cJSON_Delete(json);
        }
    }

    // Close the log file
    fclose(log_file);
}

void test_get_tcp_client_ip()
{
    // Create a mock server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_socket != -1);

    // Bind the server socket to a port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(12345); // Choose any port
    assert(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != -1);

    // Listen for connections
    assert(listen(server_socket, 5) != -1);

    // Create a client socket and connect to the server
    int client_socket = create_client_socket_and_connect("127.0.0.1", 12345);
    assert(client_socket != -1);

    // Get the client's IP address
    char client_ip[INET6_ADDRSTRLEN];
    get_tcp_client_ip(client_socket, client_ip);

    // Assert that the IP address is correct (in this case, it should be "127.0.0.1")
    assert(strcmp(client_ip, "127.0.0.1") == 0);

    // Close the client and server sockets
    close(client_socket);
    close(server_socket);
}

void test_detect_entry_valid_entries()
{
    TEST_ASSERT_EQUAL_STRING("NORTH", detect_entry("Alert in NORTH area"));
    TEST_ASSERT_EQUAL_STRING("SOUTH", detect_entry("SOUTH entry alert"));
    TEST_ASSERT_EQUAL_STRING("EAST", detect_entry("EAST side alarm"));
    TEST_ASSERT_EQUAL_STRING("WEST", detect_entry("Warning: WEST gate breached"));
}

void test_detect_entry_no_entry()
{
    TEST_ASSERT_NULL(detect_entry("No entry specified in the alert"));
}

void test_initialize_entry_alerts_count()
{
    EntryAlertsCount ealerts;
    memset(&ealerts, 0, sizeof(EntryAlertsCount)); // Clear memory before initialization
    initialize_entry_alerts_count(&ealerts);

    // Check if each member is initialized to zero
    TEST_ASSERT_EQUAL(0, ealerts.north);
    TEST_ASSERT_EQUAL(0, ealerts.south);
    TEST_ASSERT_EQUAL(0, ealerts.east);
    TEST_ASSERT_EQUAL(0, ealerts.west);
}

void test_update_emergency_info()
{
    // Create an EmergencyInfo structure
    EmergencyInfo emergency_info;

    // Call the function to update emergency information
    update_emergency_info("2024-04-18 19:11:52", "Test event", &emergency_info);

    // Check if the values are updated correctly
    TEST_ASSERT_EQUAL_STRING("2024-04-18 19:11:52", emergency_info.last_keepalived);
    TEST_ASSERT_EQUAL_STRING("Test event", emergency_info.last_event);
}

void test_get_last_keepalived()
{
    // Create an EmergencyInfo structure and initialize it with sample values
    EmergencyInfo emergency_info;
    memset(&emergency_info, 0, sizeof(EmergencyInfo));
    strcpy(emergency_info.last_keepalived, "2024-04-18 19:11:52");

    // Call the function to get the last keepalived timestamp
    const char* last_keepalived = get_last_keepalived(&emergency_info);

    // Check if the returned value matches the expected value
    TEST_ASSERT_EQUAL_STRING("2024-04-18 19:11:52", last_keepalived);
}

void test_get_last_event()
{
    // Create an EmergencyInfo structure and initialize it with sample values
    EmergencyInfo emergency_info;
    memset(&emergency_info, 0, sizeof(EmergencyInfo));
    strcpy(emergency_info.last_event, "Test event");

    // Call the function to get the last event
    const char* last_event = get_last_event(&emergency_info);

    // Check if the returned value matches the expected value
    TEST_ASSERT_EQUAL_STRING("Test event", last_event);
}

void test_get_home_dir(void)
{
    const char* homeDir = get_home_dir();
    TEST_ASSERT_NOT_NULL_MESSAGE(homeDir, "Home directory should not be NULL");
    TEST_ASSERT_MESSAGE(strlen(homeDir) > 0, "Home directory should not be empty");
}

void tearDown()
{
    // Run after all tests
}

void setUp()
{
    // Run before each test
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_add_tcp_client);
    RUN_TEST(test_get_available_port);
    RUN_TEST(test_parse_command_line_arguments);
    RUN_TEST(test_set_ports_not_specified);
    RUN_TEST(test_set_ports_tcp_specified);
    RUN_TEST(test_set_ports_udp_specified);
    RUN_TEST(test_set_ports_both_specified);
    RUN_TEST(test_convert_supplies_to_json);
    RUN_TEST(test_initiateAlertModule);
    RUN_TEST(test_cleanup_fifo);
    RUN_TEST(test_fifo_not_empty);
    RUN_TEST(test_remove_tcp_client);
    RUN_TEST(test_add_udp_client);
    RUN_TEST(test_remove_udp_client);
    RUN_TEST(test_logEvent);
    RUN_TEST(test_logEventJSON);
    RUN_TEST(test_get_tcp_client_ip);
    RUN_TEST(test_initialize_entry_alerts_count);
    RUN_TEST(test_detect_entry_valid_entries);
    RUN_TEST(test_detect_entry_no_entry);
    RUN_TEST(test_update_emergency_info);
    RUN_TEST(test_get_last_keepalived);
    RUN_TEST(test_get_last_event);
    RUN_TEST(test_get_home_dir);

    return UNITY_END();
}
