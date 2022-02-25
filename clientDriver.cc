#include <iostream>
#include <time.h>
#include "RemoteFileSystem.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fprintf(stderr, "usage: %s port\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  char *host = argv[1];
  short port = atoi(argv[2]);
  struct timeval timeout;
  timeout.tv_sec = atoi(argv[3]);
  timeout.tv_usec = atoi(argv[4]);//1 usec = 0.000001 sec
 
  unsigned long auth_token;
  {
    srand(time(0));
    for (int i=0; i<64; i++) { //64 bit number
      auth_token = auth_token*2 + rand()%2;
    }
    cout<<"auth token "<<auth_token<<endl;
    RemoteFileSystem r{host, port, auth_token, &timeout};
    
    cout<<"++++++++++++++++++++++read only file+++++++++++++++++++++++"<<endl;
    RemoteFileSystem::File* f1 = r.open("./test/test_r.txt", "r");
    char *buf = new char[100];
    f1->read(buf, 10);
    cout<<"buf: "<<buf<<endl; //buf: abcdefghij

   
    cout<<"++++++++++++++++++++++write only file++++++++++++++++++++++"<<endl;
    RemoteFileSystem::File* f2 = r.open("./test/test_w.txt", "w");
    char src [26]; //source text: abcdefg....
    int x = 97; //start from "a"
    for (int i = 0; i<26; i++){ 
      src[i] = x++;
    }
    f2->write(&src, 20);

    f1->lseek(5, SEEK_SET);
    f1->read(buf, 10);
    cout<<"buf: "<<buf<<endl; //buf: fghijkl\n ab

    char *buf2 = new char[100];
    f1->lseek(-5, SEEK_END);
    f1->read(buf2, 10);
    cout<<"buf2: "<<buf2<<endl;
  
    int rs1 = r.unlink("./test/test_w.txt");
    cout<<"unlink result: "<<(rs1 == 0? "success":"failed")<<endl;
  }
    

  {
    cout<<endl;
    for (int i=0; i<64; i++) { //64 bit number
      auth_token = auth_token*2 + rand()%2;
    }
    cout<<"auth token "<<auth_token<<endl;
    RemoteFileSystem r{host, port, auth_token, &timeout};
    
    cout<<"++++++++++++++++++++++write+ file++++++++++++++++++++++"<<endl;
    RemoteFileSystem::File* f1 = r.open("./test/test_w+.txt", "w+");

    char src [26]; //source text: abcdefg....
    int x = 97; //start from "a"
    for (int i = 0; i<26; i++){ 
      src[i] = x++;
    }

    f1->write(&src, 20);
    f1->lseek(-10, SEEK_CUR);
    char *buf = new char[100];
    f1->read(buf, 10);
    cout<<"buf: "<<buf<<endl;


    cout<<"++++++++++++++++++++++read+ file++++++++++++++++++++++"<<endl;
    int rs1 = r.rename("./test/test_r+_new.txt","./test/test_r+.txt");
    cout<<"rename result: "<<(rs1 == 0? "success":"failed")<<endl;

    RemoteFileSystem::File* f2 = r.open("./test/test_r+.txt", "r+");
    char *buf2 = new char[100];
    f2->read(buf2, 10);
    buf2[10]='\0';
    cout<<"buf2: "<<buf2<<endl;
    f2->write(&src, 10);
    
    int rs2 = r.rename("./test/test_r+.txt","./test/test_r+_new.txt");
    cout<<"rename result: "<<(rs2 == 0? "success":"failed")<<endl;

    int rs3 = r.chmod("./test/test_r.txt",S_IRUSR|S_IRGRP);
    //|S_IROTH);
    //|S_IWOTH|S_IXOTH);
    //cout<<S_IRUSR<<endl;
    //cout<<S_IRGRP<<endl;
    cout<<"chmod result: "<<(rs3 == 0? "success":"failed")<<endl;
  }
}
