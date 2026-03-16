#include "app.hpp"
#include "greet.hpp"
#include <exception>
#include <iostream>
#include <unistd.h>

const std::vector<std::string> logo =
{
    R"(  _   _                           _   _              __   __               )",
    R"( | | | |                         | \ | |             \ \ / /               )",
    R"( | |_| | __ _ _ __  _ __  _   _  |  \| | _____      __\ V /___  __ _ _ __  )",
    R"( |  _  |/ _` | '_ \| '_ \| | | | | . ` |/ _ \ \ /\ / / \ // _ \/ _` | '__| )",
    R"( | | | | (_| | |_) | |_) | |_| | | |\  |  __/\ V  V /  | |  __/ (_| | |    )",
    R"( |_| |_|\__,_| .__/| .__/ \__, | |_| \_|\___| \_/\_/   \_/\___|\__,_|_|    )",
    R"(             | |   | |     __/ |                                           )",
    R"(             |_|   |_|    |___/                                            )",
    "",
    R"(                       ✨ Wish You a Great 2026 ✨        )"
    };

constexpr RGB LOGO_START = {251, 128, 114 };  // orange
constexpr RGB LOGO_END   = {80, 200, 179};  // blue

/// Program entry point that delegates setup and execution to `tplay::App`.
/// `argc` and `argv` are the standard process command-line arguments.
int main(int argc, char** argv)
{
    show_logo(logo,LOGO_START,LOGO_END);
    sleep(2);
    try {
        tplay::App app;
        return app.run(argc, argv);
    } catch (const std::exception& ex)
        {
        std::cerr << "tplay error: " << ex.what() << "\n";
        return 1;
    }
}
