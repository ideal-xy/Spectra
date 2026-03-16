#pragma once

#include <string>

namespace tplay {

struct Track {
    std::string path;
    std::string title;
    double duration = 0.0;
};

}
