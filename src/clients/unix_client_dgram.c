#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCK_PATH "/tmp/socket"

int main()
{
    int sockfd;
    struct sockaddr_un server_addr;

    // Crear un socket Unix
    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la estructura de dirección del servidor
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCK_PATH);

    // Enviar un mensaje al servidor
    char* message = "Hello from Unix client!";
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    printf("Message sent to the server: %s\n", message);

    // Cerrar el socket
    close(sockfd);

    return 0;
}
