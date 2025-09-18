#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define RESPONCE "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello\n"

int main(void) {
  struct sockaddr_in socket_address;

  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd == -1) {
    puts("Ошибка создания сокета");
  }

  memset(&socket_address, 0, sizeof(socket_address));

  socket_address.sin_family = AF_INET;
  socket_address.sin_port = htons(PORT);
  socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockfd, (struct sockaddr *)&socket_address,
           sizeof(socket_address)) == -1) {
    perror("Ошибка: связывания");

    close(sockfd);
    exit(EXIT_FAILURE);
  }

  if (listen(sockfd, 10) == -1) {
    perror("Ошибка: прослушивания");

    close(sockfd);
    exit(EXIT_FAILURE);
  }

  for (;;) {
    int clientfd = accept(sockfd, 0, 0);

    if (clientfd == -1) {
      perror("Ошибка: принятия");
      printf("Значение clientfd: %d\n", clientfd);
      close(clientfd);
      exit(EXIT_FAILURE);
    }

    if (send(clientfd, RESPONCE, strlen(RESPONCE), 0) == -1) {
      perror("Ошибка отправки");
      close(sockfd);
      exit(EXIT_FAILURE);
    };

    shutdown(clientfd, SHUT_RDWR);

    close(clientfd);
  }
  close(sockfd);
  exit(EXIT_SUCCESS);
}
