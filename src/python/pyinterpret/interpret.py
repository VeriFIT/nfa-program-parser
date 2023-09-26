import sys
import os
import re
from enum import Enum
import pyinterpret.engines.base as engine_base
from pyinterpret.engines.mata_engine import MataEngine
from pyinterpret.engines.fado_engine import FadoEngine
from pyinterpret.engines.automatalib_engine import AutomataLibEngine
from pyinterpret.utils import timed, die

import libmata.parser as mata_parser
import libmata.alphabets as alph

number_matcher = re.compile("\d+")


class Operation(Enum):
    Intersection = 1
    Union = 2
    Complement = 3
    Emptiness = 4
    Inclusion = 5
    NaryIntersection = 6
    Concatenation = 7


def load_program(src, automata_src):
    if not os.path.exists(src):
        die(f"error: {src} does not exist")
    with open(src, 'r') as src_handle:
        lines = src_handle.read().split('\n')
    lines = [l.strip() for l in lines if l.strip()]
    program, automata = [], []
    for inst in [l.replace('(', '').replace(')', '').replace('=', '').split() for l in lines]:
        if 'load_automaton' in inst:
            assert len(inst) == 2
            automata.append(inst[1])
        elif 'load_automata' in inst:
            assert len(automata) == 0 and "Unsupported (Either load automata explicitly or call load_automata alone)"
            automata.extend([f"aut{i}" for i in range(1, len(automata_src)+1)])
        elif 'inter' in inst:
            program.append((Operation.Intersection, inst[0], inst[2:]))
        elif 'union' in inst:
            program.append((Operation.Union, inst[0], inst[2:]))
        elif 'concat' in inst:
            program.append((Operation.Concatenation, inst[0], inst[2:]))
        elif 'compl' in inst:
            program.append((Operation.Complement, inst[0], inst[2]))
        elif 'is_empty' in inst:
            program.append((Operation.Emptiness, inst[1]))
        elif 'incl' in inst:
            program.append((Operation.Inclusion, inst[1], inst[2]))
        elif 'interall' in inst:
            program.append((Operation.NaryIntersection, inst[0]))
        else:
            die(f"{' '.join(inst)} is not supported")

    return automata, program


def interpret_program(engine, program, automata_db, alphabet):
    for inst in program:
        if inst[0] == Operation.Union:
            assert len(inst[2]) == 2 and "Only binary union is currently supported"
            automata_db[inst[1]] = engine.trim(engine.union(automata_db[inst[2][0]], automata_db[inst[2][1]]))
        elif inst[0] == Operation.Intersection:
            if len(inst[2]) == 2:
                automata_db[inst[1]] = engine.trim(engine.intersection(automata_db[inst[2][0]], automata_db[inst[2][1]]))
            else:
                automata_db[inst[1]] = engine.trim(engine.intersection_all([
                    automata_db[aut] for aut in inst[2]
                ]))
        elif inst[0] == Operation.Concatenation:
            assert len(inst[2]) == 2 and "Only binary concatenation is currently supported"
            automata_db[inst[1]] = engine.trim(engine.concat(automata_db[inst[2][0]], automata_db[inst[2][1]]))
        elif inst[0] == Operation.Complement:
            automata_db[inst[1]] = engine.trim(engine.complement(automata_db[inst[2]], alphabet))
        elif inst[0] == Operation.NaryIntersection:
            automata_db[inst[1]] = engine.trim(engine.intersection_all(list(automata_db.values())))
        elif inst[0] == Operation.Inclusion:
            result = engine.inclusion(automata_db[inst[1]], automata_db[inst[2]])
            print(f"result: {result}")
        elif inst[0] == Operation.Emptiness:
            result = engine.is_empty(automata_db[inst[1]])
            print(f"result: {result}")
        else:
            die(f"unsupported operation {inst[0]}")


@timed(timer="construction")
def load_automata_db(automata_tokens, automata_sources):
    automata_to_load = []
    token_len = len(automata_tokens)
    for token in automata_tokens:
        if m := number_matcher.search(token):
            index = int(m.group(0)) - 1
            if index >= token_len:
                die(f"cannot load {token} (out of bounds)")
            automata_to_load.append(automata_sources[index])
    alphabet = alph.OnTheFlyAlphabet()
    mata_automata = mata_parser.from_mata(automata_to_load, alphabet)
    return alphabet, {
        key: aut for (key, aut) in zip(automata_tokens, mata_automata)
    }


def launch():
    if len(sys.argv) < 3:
        die(f"usage: python3 interpreter.py <ENGINE> <PROGRAM.emp> [AUT1.mata, ..., AUTn.mata]")
    engine_name, program_src, automata_src = sys.argv[1], sys.argv[2], sys.argv[3:]

    engine = engine_base.Engine.get_engine(engine_name)

    automata_tokens, program = load_program(program_src, automata_src)
    alphabet, db = load_automata_db(automata_tokens, automata_src)
    db = engine.convert_db(db, alphabet)
    if not db:
        die("automata db was not properly loaded")
    interpret_program(engine, program, db, alphabet)


if __name__ == "__main__":
    launch()
