#include "RemoteFileSystem.h"
#include <sys/time.h>

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s port\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  short port = atoi(argv[1]);
  char host[10] = "127.0.0.1"; //dummy for server
  unsigned long auth_token = 9999; //dummy for server
  struct timeval timeout; //dummy for server
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;

  RemoteFileSystem server{host, port, auth_token, &timeout};
  server.run();

}