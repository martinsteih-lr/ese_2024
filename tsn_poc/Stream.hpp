#ifndef MULTICASTSOCKET_HPP
#define MULTICASTSOCKET_HPP

#include <arpa/inet.h>
#include <array>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

namespace TSN {

enum class StreamType { Talker, Listener };
enum class CommunicationMode { Unicast, Multicast };

template <typename T>
concept StringLike = requires(T a) {
  { a.data() } -> std::same_as<char*>;
  { a.size() } -> std::same_as<std::size_t>;
};

enum class SocketError {
  RetrieveSocketError,
  BindError,
  SetPriorityError,
  SetSockOptError,
  SetMulticastInterfaceError,
  AddMembershipError,
  DropMembershipError,
  SendError,
  ReceiveError
};

template <StreamType T, CommunicationMode M = CommunicationMode::Multicast, typename Container=std::array<char, 50>>
class Stream final {
 public:
  using enum SocketError;
  template <StringLike S>
  Stream(const S& interfaceIP, const unsigned int port, const S& device,
            const S& multicastIP)
      : localSock{AF_INET,
                  htons(static_cast<uint16_t>(port)),
                  {inet_addr(interfaceIP.data())}},
        remoteSock{AF_INET,
                   htons(static_cast<uint16_t>(port)),
                   {inet_addr(multicastIP.data())}} {
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
      throw RetrieveSocketError;
    }
    ifreq ifr{};
    // for safety, null-terminate the device name
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    std::strncpy(ifr.ifr_name, device.data(), sizeof(ifr.ifr_name));
    // check if the device name is still null-terminated
    if (ifr.ifr_name[IFNAMSIZ - 1] != '\0') {
      throw BindError;
    }

    if (int result = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,
                                static_cast<void*>(&ifr), sizeof(ifr));
        result < 0) {
      throw BindError;
    }

    // Set non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
      return;
    }
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    bind();

    if constexpr (M == CommunicationMode::Multicast &&
                  T == StreamType::Listener) {
      group.imr_multiaddr.s_addr = remoteSock.sin_addr.s_addr;
      group.imr_interface.s_addr = localSock.sin_addr.s_addr;
      errno = 0;
      if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                     std::bit_cast<char*>(&group), sizeof(group)) < 0) {
        throw AddMembershipError;
      }
    } else if (M == CommunicationMode::Multicast && T == StreamType::Talker &&
               setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF,
                          std::bit_cast<char*>(&localSock.sin_addr),
                          sizeof(localSock.sin_addr)) < 0) {
      throw SetMulticastInterfaceError;
    }
  }
  Stream(const Stream&) = delete;
  Stream(Stream&&) noexcept = default;
  Stream& operator=(const Stream&) = delete;
  Stream& operator=(Stream&&) noexcept = default;

  ~Stream() {
    if constexpr (M == CommunicationMode::Multicast &&
                  T == StreamType::Listener) {
      setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                 std::bit_cast<char*>(&group), sizeof(group));
    }
    if (sockfd != -1) {
      close(sockfd);
    }
  }

  void setPriority(const unsigned int priority) const {
    if (setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &priority,
                   sizeof(priority)) < 0) {
      throw SetPriorityError;
    }
  }

  [[nodiscard]] auto receive() {
    const auto recvlen(::recvfrom(sockfd, std::begin(buffer), buffer.size()*sizeof(typename Container::value_type), 0,
                                nullptr, nullptr));
    return recvlen;
  }

  [[nodiscard]] auto send() const {
    auto size = ::sendto(sockfd, std::begin(buffer), buffer.size()*sizeof(typename Container::value_type), 0,
                         std::bit_cast<const sockaddr*>(&remoteSock),
                         sizeof(remoteSock));
    return size;
  }

  [[nodiscard]] auto& getBuffer() { return buffer; }

 private:
  void bind() const {
    using enum StreamType;
    if constexpr (T == Listener && M == CommunicationMode::Multicast) {
      if (::bind(sockfd, std::bit_cast<const sockaddr*>(&remoteSock),
                 sizeof(remoteSock)) != 0) {
        throw BindError;
      }
    } else if (T == Listener && M == CommunicationMode::Unicast) {
      if (::bind(sockfd, std::bit_cast<const sockaddr*>(&localSock),
                 sizeof(localSock)) != 0) {
        throw BindError;
      }
    } else if (T == Talker &&
               ::bind(sockfd, std::bit_cast<const sockaddr*>(&localSock),
                      sizeof(localSock)) != 0) {
      throw BindError;
    }
  }
  sockaddr_in localSock;
  sockaddr_in remoteSock;
  int sockfd;
  Container buffer{};
  std::conditional_t<M == CommunicationMode::Multicast, ip_mreq, std::tuple<>>
      group{};
};
}  // namespace TSN
#endif  // MULTICASTSOCKET_HPP