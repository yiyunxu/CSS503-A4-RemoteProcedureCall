#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream> 
#include <string>
#include <errno.h>

#include "RemoteFileSystem.h"

using namespace std;

RemoteFileSystem::RemoteFileSystem(char *host, short port, unsigned long auth_token, struct timeval *timeout) {
  this->auth_token = auth_token;
  string s = to_string(port);
  char port1[5];
  for (int i = 0; i < (int)s.length(); i++){
    port1[i] = s[i];
  }

  struct addrinfo hints;
  bzero(&hints, sizeof(struct addrinfo));
  hints.ai_flags = 0;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;
  hints.ai_addrlen = 0;
  hints.ai_addr = NULL;
  hints.ai_canonname = NULL;
  hints.ai_next = NULL;

  //struct addrinfo *addresses;
  int result = getaddrinfo(host, port1, &hints, &addresses);
  if (result != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
    exit(EXIT_FAILURE);
  }

  int n;
  struct addrinfo *p;
  for (n = 0, p = addresses; p != NULL; n++, p = p->ai_next) {
    if (p->ai_addr->sa_family != AF_INET) {
      printf("address %d: family %d  is not AF_INET(%d)", n, p->ai_addr->sa_family, AF_INET);
    } else {
      struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
      uint32_t ip = addr->sin_addr.s_addr;
      printf("address %d: %d.%d.%d.%d\n", n,
             (ip & 0xff), ((ip >> 8) & 0xff), ((ip >> 16) & 0xff), ((ip >> 24) & 0xff));
    }
  }
  printf("\n");

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    throw new RemoteSystem_Exception; //"creating socket";
  }
  
  //set timeout for client recv
  int setsocket = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)timeout,sizeof(*timeout));
  if (setsocket < 0) {  
    throw new RemoteSystem_Exception; //"setsockopt failed";
  }
  
  //marshalling into a byte message
  string tmp = to_string(99)+string("^connecting to server"); 
  const char *message = tmp.c_str();

  //send and recv
  const char* msg_receive = stub(message); 
  
  //unmarshalling
  string str = msg_receive;
  if (stoi(str) < 0) {
    throw new RemoteSystem_Exception; //"connection failed";
  }
}


RemoteFileSystem::~RemoteFileSystem() {
  //cout<<"RFS desctructor"<<endl;
  //delete file objects and close file
  while(openFileSet.size() > 0) {
    File* f = *openFileSet.begin();
    openFileSet.erase(f);
    delete f;
  }
}


const char*  RemoteFileSystem::stub(const char *p){
  unsigned long seqID = 0;
  for (int i=0; i<64; i++) { //64 bit number
      seqID = seqID*2 + rand()%2;
    }
  string tmp = p;
  tmp = to_string(auth_token) + string("^") + to_string(seqID)+ string("^") + string(tmp) + '\0';
  const char *msg_send = tmp.c_str();
  fprintf(stderr, "sending message: %s\n", msg_send);

  int result = sendto(sockfd,
                  (void *)msg_send, strlen(msg_send),
                  0,
                  addresses->ai_addr, sizeof(struct sockaddr_in));
  if (result < 0) {
    throw new RemoteSystem_Exception; //"sending to socket failed"
  }
  
  //receive
  ssize_t received = recv(sockfd, buf, MAX_MSG_SIZE, 0);
  if (received < 0) {
    if ((errno == EAGAIN) | (errno == EWOULDBLOCK)){ //timeout, send again
      fprintf(stderr, "client timeout on receive, re-sending message: %s\n", msg_send);
      result = sendto(sockfd,
                  (void *)msg_send, strlen(msg_send),
                  0,
                  addresses->ai_addr, sizeof(struct sockaddr_in));
      if (result < 0) {
        throw new RemoteSystem_Exception; //"sending to socket failed"
      }
      received = recv(sockfd, buf, MAX_MSG_SIZE, 0);
      if (received < 0){
        throw new RemoteSystem_Exception; //failed on second try
      }
    }
    else {
      throw new RemoteSystem_Exception; //"receiving from socket"
    } 
  }
  printf("received %ld characters\n", (long)received);
  buf[received] = '\0';
  printf("received message: %s\n", buf);
  printf("-------------------------------------------------------\n");

  //unmarshalling seqID
  string str(buf);
  seqID = stoul(str.substr(0, str.find("^", 0))); 
  str = str.substr(str.find("^", 0)+1, str.length()); //substring
  memcpy(buf, str.c_str(), str.length());
  buf[str.length()] = '\0';
  return buf;  
}

// Return new open file object.  Client is responsible for
// deleting.
RemoteFileSystem::File* RemoteFileSystem::open(const char *pathname, const char *mode){
  //marshalling into a byte message
  string tmp = "0^" + string(mode) + string("^") + string(pathname) ; 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = stub(msg_send); 
  //unmarshalling
  string s = msg_receive;
  if (stoi(s) < 0){
    throw new File::File_Exception; //failed to open file if fd<0
  }
  File* file = new File(this, pathname, mode);
  file->fd_ = stoi(s); //set file fd
  openFileSet.insert(file);
  return file;
}


RemoteFileSystem::File::File(RemoteFileSystem* filesystem, const char *pathname, const char *mode){
  this->filesystem = filesystem;
}


RemoteFileSystem::File::~File(){
  //closes open file
  //cout<<"file desctructor"<<endl;
  close();
}


int RemoteFileSystem::File::close(){
  //marshalling into a byte message
  string tmp = "4^" + to_string(this->fd_) ; 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = filesystem->stub(msg_send); 
  //unmarshalling
  string str = msg_receive;
  int res = stoi(str);
  return res;
}


ssize_t RemoteFileSystem::File::read(void *buf, size_t count){
  if (count > 950){
    throw new File_Exception; //"exceeds buffer size"
  }
  //marshalling into byte message
  string tmp = "1^" + to_string(this->fd_)+ string("^")+to_string(count); 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = filesystem->stub(msg_send); 
  //unmarshalling
  string str = msg_receive;
  int res = stoi(str.substr(0, str.find("^", 0)));
  str = str.substr(str.find("^", 0)+1, str.length());
  const char *text = str.c_str();
  if (res > 0){  
    memcpy(buf, text, res); //copy to buffer
  }
  return res;
}


ssize_t RemoteFileSystem::File::write(void *buf, size_t count){
  if (count > 950){
    throw new File_Exception; //"exceeds buffer size"
  }
  //marshalling into a byte message
  string tmp = "2^" + to_string(this->fd_)+ string("^")+ to_string(count) + string("^") +string((const char *)buf) ; 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = filesystem->stub(msg_send); 
  //unmarshalling
  string str = msg_receive;
  int res = stoi(str);
  return res;
}


off_t RemoteFileSystem::File::lseek(off_t offset, int whence){
  //marshalling into a byte message
  string tmp = "3^" + to_string(this->fd_)+ string("^")+ to_string(offset)+ string("^") +to_string(whence); 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = filesystem->stub(msg_send); 
  //unmarshalling
  string str = msg_receive;
  off_t res = stoi(str);
  return res;
}


int RemoteFileSystem::chmod(const char *pathname, mode_t mode){
  //marshalling into a byte message
  string tmp = "5^" + string(pathname)+ string("^") + to_string(mode); 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = stub(msg_send);
  //unmarshalling
  string str = msg_receive;
  int res = stoi(str);
  return res;
}


int RemoteFileSystem::unlink(const char *pathname){
  //marshalling into a byte message
  string tmp = "6^" + string(pathname); 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = stub(msg_send); 
  //unmarshalling
  string str = msg_receive;
  int res = stoi(str);
  return res;
}


int RemoteFileSystem::rename(const char *oldpath, const char *newpath){
  //marshalling into a byte message
  string tmp = "7^" + string(oldpath)+ string("^") + string(newpath); 
  const char *msg_send = tmp.c_str();
  //send and recv
  const char* msg_receive = stub(msg_send); 
  //unmarshalling
  string str = msg_receive;
  int res = stoi(str);
  return res;
}
