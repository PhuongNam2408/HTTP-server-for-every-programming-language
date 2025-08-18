#include <sys/socket.h>
#include <netinet/in.h> // struct sockaddr_in
#include <string.h>
#include <arpa/inet.h> // inet_ntoa()
#include <stdio.h>

int main(void) {
  int sockfd = -1, status, connfd = -1, len;
  struct sockaddr_in server_addr, client_addr;
  
  // Create a socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    printf("Socket creation failed with fd = %d\n", sockfd);
    return 1;
  } else {
    printf("Socket created\n");
  }

  memset(&server_addr, 0, sizeof(server_addr));
  
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(8080);

  status = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (status != 0) {
    printf("Socket bind failed with status = %d\n", status);
    return 1;
  } else {
    printf("Socket binded\n");
  }

  status = listen(sockfd, 5);
  if (status != 0) {
    printf("Socket listen failed with status = %d\n", status);
    return 1;
  } else {
    printf("Socket is listening at %s:%d ...\n", inet_ntoa(server_addr.sin_addr), 8080);
  }

  connfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
  if (connfd < 0 || len != sizeof(client_addr)) {
    printf("Socket accept a new connection failed with connfd = %d with received_len = %d\n", 
           connfd,
           len);
    return 1;
  } else {
    printf("Socket accepted a new connection with "
           "connfd = %d, "
           "received address len = %d, "
           "client ip address family = %u, "
           "cleint ip address = %s:%d\n",
           connfd,
           len,
           client_addr.sin_family,
           inet_ntoa(client_addr.sin_addr),
           client_addr.sin_port);
  }


  return 0;
}