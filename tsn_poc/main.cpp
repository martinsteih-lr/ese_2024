#include <CLI/CLI.hpp>
#include <array>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#include "Stream.hpp"
#include "rt_utils.hpp"

//Functionality to enhance readability and maintainability that does not need
//to show up in the final code table
namespace {
std::sig_atomic_t keepGoing{1};
constexpr unsigned rtPriority{99};
constexpr unsigned socketPriority{7};
constexpr unsigned globalCyclesAhead{20};

constexpr long cycleTimeNsecs{2'000'000};

struct data {
  long cycleCount{0};
  long localMaxJitterUs{0};
  long globalMissedPacketCount{0};
  long globalMaxJitterUs{0};
};
// Because we did not pack the struct, we must be sure that the compiler does
// not add padding.
static_assert(sizeof(data) == 4 * sizeof(long));

struct Args {
  std::string vlanInterface{};
  std::string vlanIp{};
  unsigned int port{};
  std::vector<std::string> mcastListenAddr{};
  std::string mcastSendAddress{};
  std::string cpuSetName{};
  std::string threadName{};
  int affinity{};
};

[[nodiscard]] auto parseCommandLineArguments(int argc, const char **argv)
    -> Args {
  Args args{};

  CLI::App app{"TSN receiver"};
  app.add_option("--vlan-interface", args.vlanInterface, "VLAN interface")
      ->required()
      ->description("The VLAN interface to use");
  app.add_option("--vlan-ip", args.vlanIp, "VLAN IP")
      ->required()
      ->description("The IP address of the VLAN");
  app.add_option("--port", args.port, "Port")
      ->required()
      ->description("The port to use");
  app.add_option<std::vector<std::string>>("--mcast-listen-addr",
                                           args.mcastListenAddr,
                                           "Multicast listen address")
      ->required()
      ->description("The multicast address to listen on");
  app.add_option("--mcast-send-address", args.mcastSendAddress,
                 "Multicast send address")
      ->required()
      ->description("The multicast address to send to");
  app.add_option("--cpu-set", args.cpuSetName, "CPU set name")
      ->required()
      ->description("The CPU set to use");
  app.add_option("--thread-name", args.threadName, "Thread name")
      ->required()
      ->description("The name of the thread");
  app.add_option("--affinity", args.affinity, "Affinity")
      ->required()
      ->description("The affinity to use");
  app.set_config("--config");

  app.parse(argc, argv);

  return args;
}

template <typename clock>
[[nodiscard]] auto calculateFirstWakeupTime(const long cycleTime,
                                            const unsigned cyclesAhead) {
  auto setpointWakeup(clock::now());
  // Provided a base-time of zero, calculate the shift from the 2ms cadence.
  auto cycleShift(std::chrono::duration_cast<std::chrono::nanoseconds>(
                      setpointWakeup.time_since_epoch())
                      .count() %
                  cycleTime);
  // Substract the shift, and add 20 cycles (arbitrary number) to get the a
  // clean first wakeup time according to the 2ms cadence.
  setpointWakeup -= std::chrono::nanoseconds(cycleShift);
  setpointWakeup += std::chrono::nanoseconds(cycleTimeNsecs * cyclesAhead);
  return setpointWakeup;
}

void printStats(auto latencyViolationCount, auto exchangeData,
                auto maxExecTime) {
  std::cout << "Latency violation count: " << latencyViolationCount << "\n";
  std::cout << "Cycle count: " << exchangeData.cycleCount << "\n";
  std::cout << "Local max jitter: " << exchangeData.localMaxJitterUs << "\n";
  std::cout << "Global max jitter: " << exchangeData.globalMaxJitterUs << "\n";
  std::cout << "Global missed packet count: "
            << exchangeData.globalMissedPacketCount << "\n";
  std::cout << "Max exec time: " << maxExecTime.count() << "us\n";
  std::cout << "Exiting...\n";
}
}  // namespace

int main(const int argc, const char *argv[]) {
  const auto args(parseCommandLineArguments(argc, argv));
  // Make main thread a real-time thread
  RT::setupRt(args.affinity, args.cpuSetName, args.threadName, rtPriority);

  // Set signal handler for clean shutdown
  std::signal(SIGINT, [](const int signal [[maybe_unused]]) { keepGoing = 0; });

  // Create sendig multicast socket
  TSN::Stream<TSN::StreamType::Talker>
      sendSocket(args.vlanIp, args.port, args.vlanInterface,
                 args.mcastSendAddress);
  sendSocket.setPriority(socketPriority);

  // Create receiving multicast socket
  TSN::Stream<TSN::StreamType::Listener>
      receiveSocket(args.vlanIp, args.port, args.vlanInterface,
                    args.mcastListenAddr.at(0));
  args.vlanInterface.data();

  // central definition of clock StreamType to be used
  using clock = std::chrono::system_clock;
  auto setpointWakeup(
      calculateFirstWakeupTime<clock>(cycleTimeNsecs, globalCyclesAhead));

  unsigned int latencyViolationCount(0);
  data exchangeData{};

  std::chrono::microseconds maxExecTime{0};
  do {
    std::this_thread::sleep_until(setpointWakeup);
    // Get current time to check for cheduling accuracy.
    const auto wakeupTime(clock::now());
    const auto timeDiff(std::chrono::duration_cast<std::chrono::microseconds>(
        wakeupTime - setpointWakeup));

    if (constexpr std::chrono::microseconds threshhold(20);
        timeDiff > threshhold) {
      ++latencyViolationCount;
    }
    if (auto diff(timeDiff.count()); diff > exchangeData.localMaxJitterUs) {
      exchangeData.localMaxJitterUs = diff;
    }
    // Do work here
    ++exchangeData.cycleCount;

    // for each stream, receive data and send data

    if (auto rcvSize(receiveSocket.receive());
        rcvSize == -1) {
      ++exchangeData.globalMissedPacketCount;
    } else {
      const struct data incomingData(
          *std::bit_cast<const struct data *>(std::begin(receiveSocket.getBuffer())));
      if (incomingData.globalMaxJitterUs > exchangeData.globalMaxJitterUs) {
        exchangeData.globalMaxJitterUs = incomingData.globalMaxJitterUs;
      }
      if (exchangeData.localMaxJitterUs > incomingData.globalMaxJitterUs) {
        exchangeData.globalMaxJitterUs = exchangeData.localMaxJitterUs;
      }
    }
    *std::bit_cast<struct data *>(sendSocket.getBuffer().data()) = exchangeData;
    if (auto sendSize(
            sendSocket.send());
        sendSize == -1) {
      std::cerr << "Send error\n";
    }
    auto sendTime(clock::now());
    if (auto execTime(std::chrono::duration_cast<std::chrono::microseconds>(
            sendTime - wakeupTime));
        execTime > maxExecTime) {
      maxExecTime = execTime;
    }
    // end for each stream

    // End of work
    //  Set next wakeup time
    setpointWakeup += std::chrono::nanoseconds(cycleTimeNsecs);
  } while (keepGoing != 0);
  printStats(latencyViolationCount, exchangeData, maxExecTime);
  return 0;
}
