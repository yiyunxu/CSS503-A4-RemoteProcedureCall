#include <sys/stat.h>  //open
#include <fcntl.h>     //open
#include <unistd.h>    //read
#include <stdio.h>     
#include <stdlib.h>    
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#include "RemoteFileSystem.h"

using namespace std;

// Connect to remote system.  Throw error if connection cannot be
// made.
RemoteFileSystem::RemoteFileSystem(char *host, short port, unsigned long auth_token, struct timeval *timeout) {
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    throw new RemoteSystem_Exception; //"creating socket";
  }

  struct sockaddr_in server_address;
  bzero(&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);

  int rc = bind(sockfd, (struct sockaddr *) &server_address, sizeof(server_address));
  if (rc < 0) {
    cout<<"here22"<<endl;
    throw new RemoteSystem_Exception; //"bind";
  }
  cout<<"Server constructed, waiting for request..."<<endl;
}


RemoteFileSystem::~RemoteFileSystem() {
  //server terminating, close all opening file
  while(openFDSet.size() > 0) {
    int fd = *openFDSet.begin();
    ::close(fd);
    openFDSet.erase(fd);
  }
}


void RemoteFileSystem::run(){
  for (;;) {
    char mesg[MAX_MSG_SIZE + 1];
    struct sockaddr_in client_address;
    socklen_t len = sizeof(client_address);

    int n = recvfrom(sockfd, mesg, MAX_MSG_SIZE, 0, (struct sockaddr *) &client_address, &len);
    if (n < 0) {
      throw new RemoteSystem_Exception; //"recvfrom error";
    }
    mesg[n] = 0;
    printf("-------------------------------------------------------\n");
    printf("Received from %s:%d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    printf("  \"%s\"\n", (mesg));
    
    //unmarshalling auth_token, seqID
    string str(mesg);
    auth_token = stoul(str.substr(0, str.find("^", 0))); //for authentication
    str = str.substr(str.find("^", 0)+1, str.length()); //substring

    unsigned long seqID = stoul(str.substr(0, str.find("^", 0)));
    str = str.substr(str.find("^", 0)+1, str.length()); //substring
    string tmp_msg = to_string(seqID) + string("^");  //response msg
 
    if(seqIDMap.find(seqID) == seqIDMap.end()){   //seqID not found
      //unmarshalling operation command
      int cmd = stoi(str.substr(0, str.find("^", 0)));
      cout<<"cmd: "<<cmd<<endl;
      str = str.substr(str.find("^", 0)+1, str.length()); //substring

      if (cmd == 0){ // command 0: open
        //unmarshalling
        const char* mode = (str.substr(0, str.find("^", 0))).c_str();
        str = str.substr(str.find("^", 0)+1, str.length()); //substring
        const char* filename = str.c_str(); 
        cout<<"mode: "<<mode<<endl;
        cout<<"filename: "<<filename<<endl;
        //sys call
        File* file = open(filename, mode);
        //marshalling
        tmp_msg = tmp_msg + to_string(file->fd_);
      }
      
      else if (cmd == 1){ // command 1: read
        //unmarshalling
        int fd = stoi(str.substr(0, str.find("^", 0)));
        int count = stoi(str.substr(str.find("^", 0)+1, str.length()));
        cout<<"fd: "<<fd<<endl;
        cout<<"count: "<<count<<endl;
        //sys call
        char *buf = new char[count];
        int res = ::read(fd, buf, count);
        buf[count] = '\0';
        //marshalling
        tmp_msg = tmp_msg + to_string(res) + string("^")+string((const char *)buf)+'\0';       
      }

      else if (cmd == 2){ // command 2: write
        //unmarshalling
        int fd = stoi(str.substr(0, str.find("^", 0)));
        str = str.substr(str.find("^", 0)+1, str.length()); //substring
        int count = stoi(str.substr(0, str.find("^", 0)));
        str = str.substr(str.find("^", 0)+1, str.length()); //substring
        const char *buf = str.c_str();
        cout<<"fd: "<<fd<<endl;
        cout<<"count: "<<count<<endl;
        cout<<"buf: "<<buf<<endl;
        //sys call
        int res = ::write(fd, buf, count);
        //marshalling
        tmp_msg = tmp_msg + to_string(res) +'\0';   
      }

      else if (cmd == 3){ // command 3: lseek
        //unmarshalling
        int fd = stoi(str.substr(0, str.find("^", 0)));
        str = str.substr(str.find("^", 0)+1, str.length());
        int offset = stoi(str.substr(0, str.find("^", 0)));
        str = str.substr(str.find("^", 0)+1, str.length());
        int whence = stoi(str);
        cout<<"fd: "<<fd<<endl;
        cout<<"offset: "<<offset<<endl;
        cout<<"whence: "<<whence<<endl;
        //sys call
        int res  = ::lseek(fd, offset, whence);
        //marshalling
        tmp_msg = tmp_msg + to_string(res) +'\0';
      }

      else if (cmd == 4){ // command 4: close
        //unmarshalling
        int fd = stoi(str.substr(str.find("^", 0)+1, str.length()));
        cout<<"fd: "<<fd<<endl;
        //sys call
        int res = ::close(fd);
        openFDSet.erase(fd);
        //marshalling
        tmp_msg = tmp_msg + to_string(res) +'\0';   
      }

      else if (cmd == 5){ // command 5: chmod
        //unmarshalling
        string tmp = str.substr(0, str.find("^", 0));
        const char* filename = tmp.c_str(); 
        str = str.substr(str.find("^", 0)+1, str.length());
        int mode = stoi(str);
        cout<<"filename: "<<filename<<endl;
        cout<<"mode: "<<mode<<endl;
        //sys call
        int res = ::chmod(filename, mode);
        //marshalling
        tmp_msg = tmp_msg + to_string(res) +'\0';
      }
      
      else if (cmd == 6){ // command 6: unlink
        //unmarshalling
        const char* filename = str.c_str(); 
        cout<<"filename: "<<filename<<endl;
        //sys call
        int res = ::unlink(filename);
        //marshalling
        tmp_msg = tmp_msg + to_string(res) +'\0';
      }

      else if (cmd == 7){ // command 7: rename
        //unmarshalling
        string tmp = str.substr(0, str.find("^", 0));
        const char* oldpath = tmp.c_str(); 
        str = str.substr(str.find("^", 0)+1, str.length());
        const char* newpath = str.c_str(); 
        cout<<"oldpath: "<<oldpath<<endl;
        cout<<"newpath: "<<newpath<<endl;
        //sys call
        int res = ::rename(oldpath, newpath);
        //marshalling
        tmp_msg = tmp_msg + to_string(res) +'\0';
      }

      else { // command 99: connecting to server
        tmp_msg = tmp_msg + "0" + '\0';
      }

      seqIDMap.insert({seqID, tmp_msg}); // store seqID and return msg
    }
    else{ //seqID already exist
      cout<<"Duplicated sequence ID, request had already been performed, re-sending return message"<<endl;
      tmp_msg = seqIDMap.find(seqID)->second; 
    }  
    
    const char* msg_return = tmp_msg.c_str();
    cout<<"msg_return: "<<msg_return<<endl;
    sendto(sockfd,(void *)msg_return, strlen(msg_return),
         0,(struct sockaddr *)&client_address,sizeof(client_address));   
   }
}


// Return new open file object.  Client is responsible for
// deleting.
RemoteFileSystem::File* RemoteFileSystem::open(const char *pathname, const char *mode){
  //marshalling
  int fd;
  if (*mode == 'r' && *(mode+1) == '+'){ //read+
    fd = ::open(pathname, O_RDWR);
  }
  else if (*mode == 'r'){ //read_only
    fd = ::open(pathname, O_RDONLY);
  }
  else if (*mode == 'w' && *(mode+1) == '+'){ //write+
    fd = ::open(pathname, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
  }
  else if (*mode == 'w'){ //write_only
    fd = ::open(pathname, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
  }
  else{
    fd = -1; //"open mode is not supported";
  }
  File* file = new File(this, pathname, mode);
  file->fd_ = fd; //fd is -1 on error
  if (fd > 0){
    openFDSet.insert(fd);
  }
  return file;
}


RemoteFileSystem::File::File(RemoteFileSystem* filesystem, const char *pathname, const char *mode){
}
