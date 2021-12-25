#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <experimental/coroutine>
#include <iostream>
#include <optional>

#include "async/Scheduler.h"
#include "async/Task.h"

using namespace std::chrono_literals;

constexpr static int kPort = 8080;
constexpr static size_t kListenBacklog = 3;
constexpr static size_t kReadSize = 1024;

void checkRetVal(int ret, std::string operation) {
  if (ret < 0) {
    throw std::runtime_error("Failed to " + operation + ": " +
                             std::string{strerror(errno)});
  }
}

void log(std::string message) { std::cout << message << std::endl; }

void setNonBlocking(int fd) {
  int ret = fcntl(fd, F_GETFL, 0);
  checkRetVal(ret, "fcntl()");
  ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
  checkRetVal(ret, "fcntl()");
}

Task<void> writeString(int fd, std::string value) {
  log("Started sending a string to FD " + std::to_string(fd));

  size_t bytesWritten = 0;
  while (bytesWritten < value.size()) {
    co_await Scheduler::getInstance().fd().waitToWrite(fd);
    int ret =
        ::write(fd, value.data() + bytesWritten, value.size() - bytesWritten);
    checkRetVal(ret, "write()");
    bytesWritten += ret;
  }
}

Task<void> serveClient(int clientFd) {
  log("Started serving client with FD " + std::to_string(clientFd));

  std::string pendingChars;
  char buf[kReadSize];
  while (true) {
    co_await Scheduler::getInstance().fd().waitToRead(clientFd);
    int ret = ::read(clientFd, buf, sizeof(buf));
    checkRetVal(ret, "read()");
    log("Read " + std::to_string(ret) + " bytes from FD" +
        std::to_string(clientFd));

    if (ret == 0) {
      int ret = ::close(clientFd);
      checkRetVal(ret, "close()");
      co_return;
    }

    pendingChars += std::string{buf, buf + ret};

    int lineEndIndex = pendingChars.find('\n');
    while (lineEndIndex != std::string::npos) {
      co_await writeString(clientFd, pendingChars.substr(0, lineEndIndex + 1));
      pendingChars = pendingChars.substr(lineEndIndex + 1);
      lineEndIndex = pendingChars.find('\n');
    }
  }
}

Task<void> serve() {
  int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
  checkRetVal(listenFd, "socket()");

  setNonBlocking(listenFd);

  // Set socket options
  int sockOpt = 1;
  int ret = ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &sockOpt,
                         sizeof(sockOpt));
  checkRetVal(ret, "setsockopt()");

  // Bind
  struct sockaddr_in bindAddr;
  size_t bindAddrLen = sizeof(bindAddr);
  bindAddr.sin_family = AF_INET;
  bindAddr.sin_addr.s_addr = INADDR_ANY;
  bindAddr.sin_port = htons(kPort);
  ret = ::bind(listenFd, reinterpret_cast<struct sockaddr *>(&bindAddr),
               static_cast<socklen_t>(bindAddrLen));
  checkRetVal(ret, "bind()");

  // Listen
  ret = ::listen(listenFd, kListenBacklog);
  checkRetVal(ret, "listen()");

  std::vector<Task<void>> clientTasks;

  // Loop
  while (true) {
    co_await Scheduler::getInstance().fd().waitToRead(listenFd);
    log("Got a new client in accept queue.");

    struct sockaddr_in clientAddr;
    size_t clientAddrLen;
    int clientFd =
        ::accept(listenFd, reinterpret_cast<struct sockaddr *>(&clientAddr),
                 reinterpret_cast<socklen_t *>(&clientAddrLen));
    checkRetVal(clientFd, "accept()");
    setNonBlocking(clientFd);
    log("Accepted a new client.");

    clientTasks.push_back(serveClient(clientFd));
    Scheduler::getInstance().enqueue(clientTasks.back().coro);
  }

  co_return;
}

int main() try {
  Task<void> task = serve();
  Scheduler::getInstance().enqueue(task.coro);
  Scheduler::getInstance().run();
  task.getResult();
  return 0;
} catch (const std::exception &ex) {
  std::cout << "Got exception: " << ex.what() << std::endl;
}
