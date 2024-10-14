//
// RemoteFileSystem.h
//
// Client-side remote (network) filesystem
//
// Author: Elaine Xu


#if !defined(RemoteFileSystem_H)
#define RemoteFileSystem_H

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstddef>
#include <exception>
#include <set>
#include <iostream>
#include <string.h>
#include <map>
#include <iostream> 

#define MAX_MSG_SIZE   1024

class RemoteFileSystem {
 public:
  class RemoteSystem_Exception: public std::exception {
  };
  // File represents an open file object
  class File {
  public:
    class File_Exception: public std::exception {
    };
    // Destructor closes open file.
    ~File();

   ssize_t read(void *buf, size_t count);
   ssize_t write(void *buf, size_t count);
   off_t lseek(off_t offset, int whence);
   int close();
  private:
    // Only RemoteFileSystem can open a file.
    friend class RemoteFileSystem;
    File(RemoteFileSystem* filesystem, const char *pathname, const char *mode);

    // Disallow copy & assignment
    File(File const &) = delete;
    void operator=(File const &) = delete;
  
    RemoteFileSystem* filesystem;
    int fd_;
  };

  // Connect to remote system.  Throw error if connection cannot be
  // made.
  RemoteFileSystem(char *host,
		   short port,
		   unsigned long auth_token,
		   struct timeval *timeout);

  // Disconnect
  ~RemoteFileSystem();

  // Return new open file object.  Client is responsible for
  // deleting.
  File *open(const char *pathname, const char *mode);

  int chmod(const char *pathname, mode_t mode);
  int unlink(const char *pathname);
  int rename(const char *oldpath, const char *newpath);
  void run(); //server: run function to standby and listen for request
  
 private:
  // File class may use private methods of the RemoteFileSystem to
  // implement their operations.  Alternatively, you can create a
  // separate Connection class that is local to your implementation.
  friend class File;

  // Disallow copy & assignment
  RemoteFileSystem(RemoteFileSystem const &) = delete;
  void operator=(RemoteFileSystem const &) = delete;

  char buf[MAX_MSG_SIZE+1]; //msg buffer
  int sockfd;
  struct addrinfo *addresses;
  unsigned long auth_token; //unique for each connection
  std::set<File*> openFileSet; //client: set of file* objects
  const char* stub(const char* p); //client: function to send and recv
  std::set<int> openFDSet; //server: set of opened fd's
  std::map<unsigned long, std::string> seqIDMap; //server: track seqID and msg
};

#endif
