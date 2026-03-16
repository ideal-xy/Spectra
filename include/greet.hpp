#pragma once
#include <string>
#include <vector>

struct RGB {
    int r;
    int g;
    int b;

    constexpr RGB(int r, int g, int b) : r(r), g(g), b(b) {}
    static RGB gradient(RGB start, RGB end, double ratio);
    static std::string get_viewed(RGB color);
};

void show_logo(const std::vector<std::string>& logo, const RGB& LOGO_START,const RGB& LOGO_END);