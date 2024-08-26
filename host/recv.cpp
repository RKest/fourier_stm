#define MI_IMPLEMENT
#include "receiver.hpp"

#include <iostream>
#include <ranges>
#include <string>

#define ERR(MESSAGE)                                                                               \
    {                                                                                              \
        std::cerr << MESSAGE;                                                                      \
        continue;                                                                                  \
    }

int main()
{
    mi::Receiver<5000> receiver;
    char c;
    while (std::cin.get(c))
    {
        receiver.put(c);
        if (!receiver.ready()) continue;

        auto maybe_message = receiver.collect();
        if (!maybe_message.has_value())
            ERR("Failed to collect message with error code: "
                << static_cast<uint32_t>(maybe_message.error()) << '\n');

        auto message = maybe_message.value();

        if (message.index() != mi::FourierData::id) ERR("Wrong message type\n");

        auto data = std::get<mi::FourierData>(message);

        for (auto amplitude : data.amplitudes)
            std::cout << static_cast<uint32_t>(amplitude) << " ";
        std::cerr << "Success" << '\n';
        std::cout << std::endl;
    }
}