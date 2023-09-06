#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <exception>
#include <sstream>
#include <map>
#include <vector>

enum OPERATION {
    LOAD_CMD,
    LOAD_REGEX,
    INTERSECTION,
    UNION,
    COMPLEMENT,
    DETERMINIZATION,
    EMPTINESS_CHECK,
    INCLUSION_CHECK,
    EQUIVALENCE_CHECK,
};

const std::map<std::string, OPERATION> STR_OP = {
    {"load_automaton", LOAD_CMD},
    {"load_regex", LOAD_REGEX},

    {"inter", INTERSECTION},

    {"is_empty", EMPTINESS_CHECK},
    {"incl", INCLUSION_CHECK},
};

typedef std::string Term; // variable or regex

struct SSA_Call {
    OPERATION operation;
    std::optional<Term> result;
    std::vector<Term> params;
};

std::string next_token(std::istringstream& line_stream) {
    std::string token;
    
    do {
        line_stream >> token;
    } while (token == "(" || token == ")");

    if(token.size() > 0 && token[0] == '(') {
        return token.substr(1);
    } 
    if(token.size() > 0 && token.back() == ')') {
        return token.substr(0, token.size() - 1);
    }
    return token;
}

void parse_operation(std::istringstream& line_stream, SSA_Call& result) {
    result.operation = STR_OP.at(next_token(line_stream));
    std::string token = next_token(line_stream);
    while(token != "") {
        result.params.push_back(token);
        token = next_token(line_stream);
    } 
}

std::vector<SSA_Call> parse_program(std::ifstream &input) {
    std::vector<SSA_Call> program{};
    std::string line;

    while (std::getline(input, line)) {
        line.erase(std::remove(line.begin(), line.end(), '('), line.end());
        line.erase(std::remove(line.begin(), line.end(), ')'), line.end());

        std::istringstream line_stream(line);
        std::string token;
        std::string param;
        line_stream >> token;
        if (token == "load_automaton") {
            line_stream >> param;
            program.push_back(SSA_Call{STR_OP.at(token), param, std::vector<Term>{}});
        }
        else if (token == "is_empty") {
            line_stream >> param;
            program.push_back(SSA_Call{STR_OP.at(token), {}, std::vector<Term>{param}});
        }
        else if (token == "incl") {
            SSA_Call cl{STR_OP.at(token), {}, std::vector<Term>{}};
            line_stream >> param;
            cl.params.emplace_back(param);
            line_stream >> param;
            cl.params.emplace_back(param);
            program.push_back(cl);
        } else {
            // now token is result
            SSA_Call cl{INTERSECTION, {token}, std::vector<Term>{}};
            line_stream >> token; //'='
            parse_operation(line_stream, cl);
            program.push_back(cl);
        }
    }

    return program;
}