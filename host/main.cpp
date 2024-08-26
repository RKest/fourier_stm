#define __USE_SQUARE_BRACKETS_FOR_ELEMENT_ACCESS_OPERATOR
#include "simple_fft/fft.hpp"

#define MI_IMPLEMENT
#include "include/message_definitions.hpp"
#include "include/to_string.hpp"
#include "sender.hpp"
#include <fstream>
#include <iostream>
#include <syncstream>
#include <thread>

using namespace mi;

using SetFreqDataSender = Sender<255>;
using HeartbeatSender = Sender<10>;

void print_error(std::optional<Error> err);
void broadcast_heartbeat(HeartbeatSender& comm);
void setFreqData(SetFreqDataSender& comm);

template<typename T> auto prompt(std::string_view prompt) -> T
{
    std::osyncstream{std::cerr} << prompt;
    T input;
    std::cin >> input;
    return input;
}

class Out
{
public:
    void put(uint8_t value) { ss << value; }

    void flush()
    {
        std::osyncstream{std::cout} << ss.str() << std::flush;
        ss.str("");
        ss.clear();
    }

private:
    std::stringstream ss;
};

static Out heartbeat_out;
static Out set_freq_out;

int main()
{
    bool run = true;
    HeartbeatSender heartbeat_comm{([](uint8_t byte) { heartbeat_out.put(byte); })};
    SetFreqDataSender comm{([](uint8_t byte) { set_freq_out.put(byte); })};
    const std::jthread heartbeat_thread{
        [&heartbeat_comm, &run]()
        {
            while (run)
            {
                broadcast_heartbeat(heartbeat_comm);
                std::this_thread::sleep_for(std::chrono::seconds{1});
            }
        }};

    while (run)
    {
        switch (prompt<uint32_t>("Enter command\n"
                                 "Set freq data:\t0\n"
                                 "Exit:\t\t1\n"))
        {
        case 0: setFreqData(comm); break;
        case 1: run = false; break;
        }
    }
}

void broadcast_heartbeat(HeartbeatSender& comm)
{
    static uint8_t next_seq = 0;
    Heartbeat next_heartbeat{next_seq++};
    std::optional<Error> const err = comm.send(next_heartbeat);
    print_error(err);
    heartbeat_out.flush();
}

void setFreqData(SetFreqDataSender& comm)
{
    auto min_freq = prompt<uint32_t>("Enter min frequency: ");
    auto step_freq = prompt<float>("Enter step frequency: ");
    SetFrequencyData data{min_freq, step_freq};
    std::optional<Error> const err = comm.send(data);
    print_error(err);
    set_freq_out.flush();
}

void print_error(std::optional<Error> err)
{
    if (err && err.value() != Error::NO_ERROR)
    {
        std::osyncstream{std::cerr} << "Encountered error: " << static_cast<unsigned>(err.value())
                                    << '\n';
    }
}
