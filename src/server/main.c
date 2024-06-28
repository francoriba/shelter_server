#include "server.h"
#include <stdio.h>

int main(int argc, char* argv[])
{

    // Set up signal handling for SIGINT (Ctrl+C)
    struct sigaction sig;
    sig.sa_flags = SA_SIGINFO;
    sig.sa_handler = sigint_handler;
    sigemptyset(&(sig.sa_mask));
    sigaction(SIGINT, &sig, NULL);

    // DEFAULT_PORT = -1. Alternatively, DEFAULT_PORT can be set to a specific port (Ex 5005)
    int tcp_port = DEFAULT_PORT;
    int udp_port = DEFAULT_PORT;

    parse_command_line_arguments(argc, argv, &tcp_port, &udp_port);

    // If only UDP port is specified, assign an available port to TCP
    if (tcp_port == DEFAULT_PORT && udp_port != DEFAULT_PORT)
    {
        tcp_port = get_available_port();
    }
    // If only TCP port is specified, assign an available port to UDP
    else if (tcp_port != DEFAULT_PORT && udp_port == DEFAULT_PORT)
    {
        udp_port = get_available_port();
    }
    // If both ports are not specified, assign available ports to both
    else if (tcp_port == DEFAULT_PORT && udp_port == DEFAULT_PORT)
    {
        tcp_port = get_available_port();
        udp_port = get_available_port();
    }

    srand((unsigned int)time(NULL));

    printf(" _       __     __                             _____            ___       __          _     \n");
    printf("| |     / /__  / /________  ____ ___  ___     / ___/__  _______/   | ____/ /___ ___  (_)___ \n");
    printf("| | /| / / _ \\/ / ___/ __ \\/ __ `__ \\/ _ \\    \\__ \\/ / / / ___/ /| |/ __  / __ `__ \\/ / __ \\\n");
    printf("| |/ |/ /  __/ / /__/ /_/ / / / / / /  __/   ___/ / /_/ (__  ) ___ / /_/ / / / / / / / / / /\n");
    printf("|__/|__/\\___/_/\\___/\\____/_/ /_/ /_/\\___/   /____/\\__, /____/_/  |_|\\__,_/_/ /_/ /_/_/_/ /_/\n");
    printf("                                                 /____/                                     \n");

    printf("TCP port being used: %d\n", tcp_port);
    printf("UDP port being used: %d\n", udp_port);
    pid_t pid = getpid();
    // printf("Main process PID: %d\n", pid);

    create_infection_alerts_process();
    create_power_outage_alert_process();
    start_server(tcp_port, udp_port);
    return 0;
}
