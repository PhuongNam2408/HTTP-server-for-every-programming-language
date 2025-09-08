#include <sys/socket.h>
#include <netinet/in.h> // struct sockaddr_in
#include <string.h>
#include <arpa/inet.h> // inet_ntoa()
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h> // open()
#include <unistd.h> // dup2()

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define HTTP_PORT_DEFAULT (8080)
#define HTTP_MAX_SOCKET_CONNECTIONS (10)

#define CHECK_STATUS_MINUS_1_LOG_PERROR_RETURN(status, success_msg, error_msg) \
do {\
  if (status == -1) {                \
    fprintf(stderr, "ERROR: %s - %s\n", error_msg, strerror(errno));\
    return -1;                        \
  } else {                            \
    printf("%s\n", success_msg);      \
  }\
} while(0);

#define CHECK_STATUS_MINUS_1_PERROR(status, error_msg) \
do {\
  if (status == -1) {                \
    fprintf(stderr, "ERROR: %s - %s\n", error_msg, strerror(errno));\
  }\
} while(0);

#define CHECK_STATUS_NOT_EQUAL_0_LOG_PERROR_RETURN(status, success_msg, error_msg) \
do {\
  if (status != 0) {                \
    fprintf(stderr, "ERROR: %s - %s\n", error_msg, strerror(errno));\
    return -1;                        \
  } else {                            \
    printf("%s\n", success_msg);      \
  }\
} while(0);

// Struct for storing connection's information
typedef struct {
  int fd;
  struct sockaddr_in address;
  pthread_t thread_id;
} client_info_t;

// Struct for storing multiple client connections
typedef struct {
  client_info_t data[HTTP_MAX_SOCKET_CONNECTIONS];
  int num_of_connecting_client;
} client_info_list_t;

#define UNUSED_PARAMETER(param) ((void) param)

#define CLIENT_BUFFER_SIZE_MAX (1024)

#define HEX_DUMP_BUFFER_SIZE_MAX (4096)
/*******************************************************************************
 *************************** GLOBAL VARIABLES   ********************************
 ******************************************************************************/
static client_info_list_t client_info_list = {
  .num_of_connecting_client = 0,
};
 /*******************************************************************************
 ****************************  Local Function   ********************************
 ******************************************************************************/
static void hex_dump(char *start_message, char *buff_hex, size_t buff_size)
{
  // <start_message><XXXXXXXX...>\nNULL
  if (buff_size * 2 + strlen(start_message) + 2 > HEX_DUMP_BUFFER_SIZE_MAX) {
    printf("Size too large, buff_size = %lu, start_message len = %lu\n", buff_size, strlen(start_message));
    return;
  }

  char buffer[HEX_DUMP_BUFFER_SIZE_MAX];
  int ret_size;
  size_t buffer_index = 0;

  strncpy(buffer, start_message, HEX_DUMP_BUFFER_SIZE_MAX);

  buffer_index += strlen(start_message);

  for(int i = 0; i < buff_size; i++) {
    ret_size = snprintf(buffer + buffer_index, HEX_DUMP_BUFFER_SIZE_MAX, "%02X", buff_hex[i]);
    buffer_index += ret_size;
  }

  snprintf(buffer + buffer_index, HEX_DUMP_BUFFER_SIZE_MAX, "\n");

  printf("%s", buffer);
}

 /*******************************************************************************
 ****************************  Local Function   ********************************
 ******************************************************************************/
/**
 * @brief thread handler for each connection from a client
 * 
 * Receive the data from client and log to console
 */
static void* client_connection_thread_handler(void *arg)
{
  int client_index = *((int *)arg);
  size_t ret_size;
  int client_conn_fd = client_info_list.data[client_index].fd;
  char buffer[CLIENT_BUFFER_SIZE_MAX];
  printf("Receiving data from client_index = %d, thread_id = %lu, fd = %d...\n",
         client_index,
         client_info_list.data[client_index].thread_id,
         client_conn_fd);

  while (1) {
    // Read all the data of the client_fd, print it out
    ret_size = recv(client_conn_fd, buffer, CLIENT_BUFFER_SIZE_MAX, 0);
    CHECK_STATUS_MINUS_1_PERROR(ret_size, 
                                "recv from client failed");
    if (ret_size == 0) {;
      // Connection with client has been closed. We should delete the thread and clean all necessary resource
      printf("Connection with client_index = %d has been closed!!!\n", client_index);
      return NULL;
    } else {
      buffer[ret_size] = '\0';
      printf("\n----------------BEGIN RECEIVED DATA----------------\n");
      printf("%s", buffer);
      hex_dump("\nStart HEX DUMP: ", buffer, strlen(buffer));
      printf("\n-----------------END RECEIVED DATA----------------\n");
    }
  }
}

 /*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
int main(void) {
  int sockfd = -1, status, connfd = -1, log_fd;
  socklen_t len;
  struct sockaddr_in server_addr, client_addr;
  const int enable_reuseaddress = 1;
  int thread_data_list[HTTP_MAX_SOCKET_CONNECTIONS];

  log_fd = open("log.txt", O_CREAT | O_RDWR | O_TRUNC);
  CHECK_STATUS_MINUS_1_LOG_PERROR_RETURN(log_fd,
                                         "open log.txt success",
                                         "open log.txt failed");

  status = dup2(log_fd, 1);
  CHECK_STATUS_MINUS_1_LOG_PERROR_RETURN(status,
                                         "dup2 stdout success",
                                         "dup2 stdout failed");
  
  status = close(log_fd);
  CHECK_STATUS_MINUS_1_LOG_PERROR_RETURN(status,
                                         "close log_fd success",
                                         "close log_fd failed");
  
  // Create a socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    printf("Socket creation failed with fd = %d\n", sockfd);
    return -1;
  } else {
    printf("Socket created\n");
  }

  // Setsockopt for reusing Address and port
  status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable_reuseaddress, sizeof(enable_reuseaddress));
  CHECK_STATUS_MINUS_1_LOG_PERROR_RETURN(status, 
                                         "setsockopt with SO_REUSEADDR success", 
                                         "setsockopt with SO_REUSEADDR failed");

  memset(&server_addr, 0, sizeof(server_addr));
  
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(HTTP_PORT_DEFAULT);

  status = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  CHECK_STATUS_MINUS_1_LOG_PERROR_RETURN(status, 
                                         "bind success", 
                                         "bind failed");

  status = listen(sockfd, HTTP_MAX_SOCKET_CONNECTIONS);
  CHECK_STATUS_MINUS_1_LOG_PERROR_RETURN(status, 
                                         "listen success", 
                                         "listen failed");

  printf("Socket is listening at %s:%d ...\n", inet_ntoa(server_addr.sin_addr), HTTP_PORT_DEFAULT);

  while (1) {
    connfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
    if (connfd < 0 || len != sizeof(client_addr)) {
      printf("Socket accept a new connection failed with connfd = %d with received_len = %u\n", 
            connfd,
            len);
      return -1;
    } else {
      int client_index = client_info_list.num_of_connecting_client;
      thread_data_list[client_index] = client_index;

      printf("Socket accepted a new connection with "
            "connfd = %d, "
            "received address len = %u, "
            "client ip address family = %u, "
            "client ip address = %s:%d\n",
            connfd,
            len,
            client_addr.sin_family,
            inet_ntoa(client_addr.sin_addr),
            client_addr.sin_port);
      
      client_info_list.data[client_index].fd = connfd;
      memcpy(&client_info_list.data[client_index].address, &client_addr, sizeof(client_addr));

      status = pthread_create(&client_info_list.data[client_index].thread_id,
                              NULL,
                              (void *)client_connection_thread_handler,
                              &thread_data_list[client_index]);
      CHECK_STATUS_NOT_EQUAL_0_LOG_PERROR_RETURN(status,
                                                 "Pthread create success",
                                                 "Pthread create failed");

      client_info_list.num_of_connecting_client++;
    }
  }

  return 0;
}