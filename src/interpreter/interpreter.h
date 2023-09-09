
#ifndef _INTERPRETER_H_
#define _INTERPRETER_H_

#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <exception>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <stdexcept>

#include <mata/nfa/nfa.hh>
#include <mata/parser/inter-aut.hh>
#include <mata/parser/mintermization.hh>

#include "parser.h"

template<typename NFA>
struct Instance {
    std::function<NFA(const mata::IntermediateAut&)> mata_to_nfa;
    std::function<NFA(const NFA&, const NFA&)> intersection;
    std::function<NFA(const NFA&, const NFA&)> uni;
    std::function<bool(const NFA&)> is_empty;
    std::function<bool(const NFA&, const NFA&)> is_included;
};

template<typename NFA>
struct Interpreter {

    int param_cnt = 0;
    Instance<NFA> instance {};
    std::vector<SSA_Call> program {};
    std::map<std::string, NFA> aut_table {};
    std::vector<std::string> arguments;


    Interpreter(const Instance<NFA>& inst, const std::vector<std::string>& args) : instance(inst), param_cnt(0), program(), aut_table(), arguments(args) { }

public:
    void run_program(std::ifstream& filename) {
        this->program = parse_program(filename);
        this->param_cnt = 0;

        for(size_t i = 0; i < this->program.size(); i++) {
            switch(this->program[i].operation) {
            case LOAD_CMD:
                load_nfa(this->program[i].result.value(), this->arguments[this->param_cnt++]);
                break;
            case INTERSECTION:
                intersection(this->program[i].result.value(), this->program[i].params);
                break;
            case UNION:
                uni(this->program[i].result.value(), this->program[i].params);
                break;
            case EMPTINESS_CHECK:
                is_empty(this->program[i].params[0]);
                break;
            case INCLUSION_CHECK:
                is_included(this->program[i].params[0], this->program[i].params[1]);
                break;
            }
        }
    }

private:
    void load_nfa(const std::string& name, const std::string& filename) {
        std::fstream fs(filename, std::ios::in);
        if (!fs) {
            throw std::runtime_error("could not open file");
        }

        mata::parser::Parsed parsed;
        parsed = mata::parser::parse_mf(fs, true);
        fs.close();

        if (parsed.size() != 1) {
            throw std::runtime_error("The number of sections in the input file is not 1\n");
        }
        if (!parsed[0].type.starts_with(mata::nfa::TYPE_NFA)) {
            throw std::runtime_error("The type of input automaton is not NFA\n");
        }

        std::vector<mata::IntermediateAut> inter_auts = mata::IntermediateAut::parse_from_mf(parsed);
        mata::IntermediateAut inter_aut;

        if(inter_aut.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR) {
            mata::Mintermization mintermization;
            inter_aut = mintermization.mintermize(inter_auts)[0];
        } else {
            inter_aut = inter_auts[0];
        }

        this->aut_table[name] = this->instance.mata_to_nfa(inter_aut);
    }

    void intersection(const std::string& res, const std::vector<std::string>& params) {
        assert(params.size() > 0);
        NFA tmp = this->aut_table.at(params[0]);
        for(size_t i = 1; i < params.size(); i++) {
            tmp = this->instance.intersection(tmp, this->aut_table.at(params[i]));
        }
        this->aut_table[res] = tmp;
    }

    void uni(const std::string& res, const std::vector<std::string>& params) {
        assert(params.size() > 0);
        NFA tmp = this->aut_table.at(params[0]);
        for(size_t i = 1; i < params.size(); i++) {
            tmp = this->instance.uni(tmp, this->aut_table.at(params[i]));
        }
        this->aut_table[res] = tmp;
    }

    void is_included(const std::string& a1, const std::string& a2) {
        this->instance.is_included(this->aut_table.at(a1), this->aut_table.at(a2));
    }

    void is_empty(const std::string& name) {
        this->instance.is_empty(this->aut_table.at(name));
    }
};

#endif
