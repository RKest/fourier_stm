#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct
{
    std::size_t lines = 1;
    float max = std::numeric_limits<float>::infinity();
    float min = std::numeric_limits<float>::lowest();
} args;

void help(std::string_view program)
{
    std::cout << "Example Usage: echo \"1 2 3\" | " << program
              << " [options]\n"
                 "Options:\n"
                 "  -l, --lines <lines>  Number of lines to display\n"
                 "  --max <max>          Maximum value\n"
                 "  --min <min>          Minimum value\n"
                 "  --help               Display this message\n";
    exit(EXIT_FAILURE);
}

void parse_args(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        if (arg == "-l" || arg == "--lines")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing argument for -l\n";
                exit(EXIT_FAILURE);
            }
            args.lines = std::stoul(argv[++i]);
        }
        else if (arg == "--max")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing argument for --max\n";
                exit(EXIT_FAILURE);
            }
            args.max = std::stof(argv[++i]);
        }
        else if (arg == "--min")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing argument for --min\n";
                exit(EXIT_FAILURE);
            }
            args.min = std::stof(argv[++i]);
        }
        else
        {
            help(argv[0]);
        }
    }
}

auto collect() -> std::vector<float>
{
    std::vector<float> values;
    std::string value;
    float max = std::numeric_limits<float>::lowest();
    while (std::cin >> value)
    {
        auto f = std::stof(value);
        if (f < args.min) f = 0;
        max = std::max(max, f);
        values.push_back(f);
    }
    for (auto& f : values)
    {
        f /= std::isfinite(args.max) ? args.max : max;
        f *= args.lines;
    }
    return values;
}

constexpr std::string_view blocks = "▁▂▃▄▅▆▇█";
constexpr std::size_t unicode_size = sizeof("▂") - sizeof('\0');
constexpr std::size_t n_blocks = blocks.size() / unicode_size;
auto block(std::size_t index) -> std::string
{
    index -= 1;
    if (index > std::numeric_limits<std::size_t>::max() / 2) return " ";
    index = std::min(index, n_blocks - 1);
    const static std::string str_blocks{blocks};
    assert(index < n_blocks);
    std::size_t inx = index * unicode_size;
    return str_blocks.substr(inx, unicode_size);
}

int main(int argc, char** argv)
{
    parse_args(argc, argv);
    auto values = collect();
    for (std::size_t line = args.lines; line != 0; --line)
    {
        for (auto f : values)
        {
            std::size_t scalar = (f - line + 1) * n_blocks;
            std::cout << block(scalar);
        }
        std::cout << '\n';
    }
}