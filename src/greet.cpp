#include "greet.hpp"
#include <iostream>

std::string RGB::get_viewed(const RGB color)
{
    return "\033[38;2;" + std::to_string(color.r) + ";" +
          std::to_string(color.g) + ";" + std::to_string(color.b) + "m";
}

void show_logo(const std::vector<std::string>& logo, const RGB& LOGO_START, const RGB& LOGO_END)
{
    for (size_t i = 0; i < logo.size(); ++i)
    {
        const double ratio = static_cast<double>(i) / static_cast<int>(logo.size() - 1);
        const RGB row_rgb = RGB::gradient(LOGO_START, LOGO_END, ratio);
        std::cout << RGB::get_viewed(row_rgb) << logo[i] << "\033[0m" << std::endl;
    }
    std::cout << std::endl;
}

RGB RGB::gradient(const RGB start,const RGB end,const double ratio)
{
    return {
        static_cast<int>(start.r + (end.r - start.r) * ratio),
        static_cast<int>(start.g + (end.g - start.g) * ratio),
        static_cast<int>(start.b + (end.b - start.b) * ratio)
    };
}
