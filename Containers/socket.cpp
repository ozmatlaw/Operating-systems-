#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>



#define MAKE_SOCKET_FAIL "system error: make socket function has failed\n"
#define NO_PORT "system error: no port provided\n"
#define HOST_NAME_FAIL "system error: failed to get host name\n"
#define BIND_FAIL "system error: failed to bind socket\n"
#define LISTEN_FAIL "system error: failed to listen to socket\n"
#define READ_FAIL "system error: failed to read the input\n"
#define WRITE_FAIL "system error: failed to write the input\n"
#define MAX_HOST_NAME 256
#define BUFFER_SIZE 256
#define CLIENT_STR "client"

enum Type{ClIENT, SERVER};

void systemCallFailed(const std::string& str)
{
  std::cerr << str;
  exit(EXIT_FAILURE);
}

int establishClient(unsigned short portNum, char* terminalCommand) {

  struct hostent *hp;
  char Command[BUFFER_SIZE];
  char myName[MAX_HOST_NAME + 1]; //todo what is the value for the max?
  gethostname(myName, MAX_HOST_NAME);
  hp = gethostbyname(myName);
  if (hp == nullptr) {
    systemCallFailed(HOST_NAME_FAIL);
    exit(1);
  }
  // sockaddrr_in init
  sockaddr_in sa;
  memset(&sa,0,sizeof(sockaddr_in));
  memcpy((char*)&sa.sin_addr,hp->h_addr,hp->h_length);
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons((u_short)portNum);
  // make socket
  int sending = socket(hp->h_addrtype,SOCK_STREAM,0);
  if(sending < 0){
    systemCallFailed(MAKE_SOCKET_FAIL);
  }
  // connect to server
  if(connect(sending,(struct sockaddr*)&sa,sizeof(sa))<0){
    systemCallFailed(MAKE_SOCKET_FAIL);
  }
  memset(&Command, 0, BUFFER_SIZE);
  strncpy(Command, terminalCommand, BUFFER_SIZE - 1);
  if ((write(sending, Command, strlen(Command))) < 0) {
    systemCallFailed(WRITE_FAIL);
  }
  close(sending);
  return 0;
}

int readInput(int client_socket_fd, char *buffer, int n) {
  int count = 0, br = 0;
  while (count < n) {
    br = read(client_socket_fd, buffer, n - count);
    if (br < 0) {
      return br;
    }
    else if (br > 0) {
      buffer += br;
      count += br;
    } else {
      return count;
    }
  }
  return count;
}

int establishServer(unsigned short portNum){
  //get host name
  struct hostent *hp;
  char myName[MAX_HOST_NAME + 1];
  char terminalCommand[BUFFER_SIZE];
  gethostname(myName, MAX_HOST_NAME);
  hp = gethostbyname(myName);
  if (hp == nullptr) {
    systemCallFailed(HOST_NAME_FAIL);
    exit(1);
  }
  // sockaddrr_in init
  sockaddr_in hint;
  int listening;
  memset(&hint,0,sizeof(struct sockaddr_in));
  hint.sin_family = hp->h_addrtype;
  memcpy(&hint.sin_addr,hp->h_addr,hp->h_length);
  hint.sin_port = htons(portNum);
  // create socket
  if((listening = socket(AF_INET,SOCK_STREAM,0))<0){
    systemCallFailed(MAKE_SOCKET_FAIL);
    exit(1);
  }
  //bind socket
  if(bind(listening, (struct sockaddr *)&hint, sizeof(struct sockaddr_in))<0){
    systemCallFailed(BIND_FAIL);
    exit(1);
  }
  //start listening for clients
  if (listen(listening,5) < 0) {
    systemCallFailed(LISTEN_FAIL);
    exit(1);
  }
  sockaddr_in severClient;
  while(true){
    socklen_t clientSize = sizeof (severClient);
    int clientSocket = accept(listening, (sockaddr*)&severClient, &clientSize);
    if(clientSocket < 0){
      systemCallFailed(MAKE_SOCKET_FAIL);
    }
    memset(terminalCommand, 0, BUFFER_SIZE);
    if (readInput(clientSocket, terminalCommand, BUFFER_SIZE -1) < 0) {
      systemCallFailed(READ_FAIL);
    }
    system(terminalCommand);
    close(clientSocket);
  }
  close(listening);
}


int main(int argc, char **argv) {
  if (argc < 2) {
    systemCallFailed(NO_PORT);
    exit(1);
  }
  auto type = strcmp(argv[1], CLIENT_STR) ==0? ClIENT: SERVER;
  int portNum = atoi(argv[2]);
  
  std::string curTerminalCommand;
  for (int i = 3; i < argc; ++i) {
    curTerminalCommand += argv[i];
    curTerminalCommand += " ";
  }
  char* terminalCommand = const_cast<char*>(curTerminalCommand.c_str());

  switch (type) {
    case ClIENT:
      establishClient(portNum,terminalCommand);
      break;
    case SERVER:
      establishServer(portNum);
      break;
  }
  return 0;
}
