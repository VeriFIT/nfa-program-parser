#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <exception>
#include <sstream>
#include <map>
#include <vector>

#include "parser.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "error: Program expects exactly one argument, try '--help' for help" << std::endl;
        return -1;
    }

    std::string arg = std::string(argv[1]);

    if (arg == "-h" || arg == "--help") {
        std::cout << "Usage: nfa-emptiness-checker input.emp" << std::endl;
        return 0;
    }

    std::ifstream input(arg);
    if (!input.is_open()) {
        std::cerr << "error: could not open file " << arg << std::endl;
        return -1;
    }

    try {
        std::vector<SSA_Call> program = parse_program(input);

        // TODO: so-far just print the program
        for(const auto& t : program) {
            std::cout << t.operation << ": " << t.result.value_or("") << " <- ";
            for(const auto &m : t.params) {
                std::cout << m << " ";
            }
            std::cout << std::endl;
        }

    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}