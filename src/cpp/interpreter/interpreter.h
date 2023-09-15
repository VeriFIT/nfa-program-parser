
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
#include "../utils/util.h"

template<typename NFA>
struct Instance {
    std::function<NFA(const mata::IntermediateAut&, const std::string& filename)> mata_to_nfa;
    std::function<NFA(const NFA&, const NFA&)> intersection;
    std::function<NFA(const std::vector<NFA>)> inter_all;
    std::function<NFA(const NFA&, const NFA&)> uni;
    std::function<NFA(const NFA&)> complement;
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
    std::vector<std::string> load_all_auts {};
    std::vector<mata::IntermediateAut> inter_auts {};


    Interpreter(const Instance<NFA>& inst, const std::vector<std::string>& args) : instance(inst), param_cnt(0), program(), aut_table(), arguments(args) { }

public:
    void run_program(std::ifstream& filename) {
        this->program = parse_program(filename);
        this->param_cnt = 0;

        // load and minterminize all input automata
        prepare_intermediate_automata();

        for(size_t i = 0; i < this->program.size(); i++) {
            switch(this->program[i].operation) {
            case LOAD_CMD:
                load_nfa(this->program[i].result.value(), this->param_cnt++);
                break;
            case LOAD_ALL: // all automata are already loaded
                load_all();
                break;
            case INTERSECTION:
                intersection(this->program[i].result.value(), this->program[i].params);
                break;
            case INTERALL:
                inter_all(this->program[i].result.value());
                break;
            case UNION:
                uni(this->program[i].result.value(), this->program[i].params);
                break;
            case COMPLEMENT:
                complement(this->program[i].result.value(), this->program[i].params);
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
    void prepare_intermediate_automata() {
        std::vector<mata::IntermediateAut> auts {};
        bool exists_bv = false;
        TIME_BEGIN(mataparsing);
        for(const std::string& fl : this->arguments) {
            mata::IntermediateAut aut = load_intermediate(fl);
            exists_bv = exists_bv || aut.alphabet_type == mata::IntermediateAut::AlphabetType::BITVECTOR;
            auts.push_back(aut);
        }
        TIME_END(mataparsing);
        if(exists_bv) {
            TIME_BEGIN(mataminterm);
            mata::Mintermization mintermization;
            this->inter_auts = mintermization.mintermize(auts);
            TIME_END(mataminterm);
        } else {
            this->inter_auts = auts;
        }
    }

    mata::IntermediateAut load_intermediate(const std::string& filename) {
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

        return mata::IntermediateAut::parse_from_mf(parsed)[0];
    }

    void load_nfa(const std::string& name, int index) {
        this->aut_table[name] = this->instance.mata_to_nfa(this->inter_auts[index], this->arguments[index]);
    }

    void load_all() {
        for(size_t i = 0; i < this->inter_auts.size(); i++) {
            std::string name = "_aut{}" + std::to_string(i);
            this->aut_table[name] = this->instance.mata_to_nfa(this->inter_auts[i], this->arguments[i]);
            this->load_all_auts.push_back(name);
        }
    }

    void inter_all(const std::string& res) {
        std::vector<NFA> nfas {};
        for(const std::string& name : this->load_all_auts) {
            nfas.emplace_back(this->aut_table[name]);
        }
        this->aut_table[res] = this->instance.inter_all(nfas);
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

    void complement(const std::string& res, const std::vector<std::string>& params) {
        assert(params.size() == 1);
        NFA tmp = this->aut_table.at(params[0]);
        this->aut_table[res] = this->instance.complement(tmp);
    }

    void is_included(const std::string& a1, const std::string& a2) {
        this->instance.is_included(this->aut_table.at(a1), this->aut_table.at(a2));
    }

    void is_empty(const std::string& name) {
        this->instance.is_empty(this->aut_table.at(name));
    }
};

#endif
