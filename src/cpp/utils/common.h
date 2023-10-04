#ifndef _UTIL_COMMON_H_
#define _UTIL_COMMON_H_

#include <string>
#include <vector>

struct Params {
    bool sim_red = false;
    bool det = false;

    std::string program;
    std::vector<std::string> automata {};
};


Params parse_input_args(int argc, char** argv) {
    Params ret {};
    ret.program = std::string(argv[1]);
    for(size_t i = 2; i < argc; i++) {
        std::string arg = std::string(argv[i]);
        if(arg == "--sim") {
            ret.sim_red = true;
            continue;
        }
        if(arg == "--det") {
            ret.det = true;
            continue;
        }
        ret.automata.push_back(std::string(argv[i]));
    }
    return ret;
}



#endif
