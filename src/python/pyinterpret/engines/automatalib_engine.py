from typing import Any
from collections import defaultdict

import pyinterpret.engines.base as engine_base
from pyinterpret.utils import timed, die

import automata.fa.nfa as nfa
import automata.fa.dfa as dfa


def parse_targets(aut, state):
    targets = defaultdict(set)
    for trans in aut.get_transitions_from_state(state):
        targets[trans.symbol].update(trans.targets)
    return targets


def parse_states_and_trans(aut):
    transitions = {}
    states = set()
    for trans in aut.iterate():
        if trans.source not in transitions.keys():
            transitions[trans.source] = defaultdict(list)
        states.add(trans.source)
        states.add(trans.target)
        transitions[trans.source][trans.symbol].append(trans.target)
    return states, transitions


class AutomataLibEngine(engine_base.Engine):
    def trim(self, lhs: Any):
        return lhs

    @timed(timer="conversion")
    def convert_db(self, db: dict, alphabet) -> Any:
        for token, aut in db.items():
            aut.unify_initial()
            aut_initial_states = aut.initial_states
            aut_states, transitions = parse_states_and_trans(aut)
            if len(aut_initial_states) != 1:
                die(f"multiple initial states got ({len(aut_initial_states)}")

            states = {f"q{i}" for i in aut_states}
            input_symbols = {f"{s}" for s in alphabet.get_alphabet_symbols()}
            transitions = {
                f"q{state}": {
                    f"{symbol}": {f"q{t}" for t in targets} for (symbol, targets) in transitions[state].items()
                }
                for state in aut_states if state in transitions.keys()
            }
            initial_state = f"q{aut_initial_states[0]}"
            final_states = {f"q{f}" for f in aut.final_states}
            nfa_aut = nfa.NFA(
                states=states,
                input_symbols=input_symbols,
                transitions=transitions,
                initial_state=initial_state,
                final_states=final_states
            )
            db[token] = dfa.DFA.from_nfa(nfa_aut, minify=False)
        return db

    @timed(timer="intersection")
    def intersection(self, lhs: dfa.DFA, rhs: dfa.DFA) -> Any:
        return lhs.intersection(rhs, minify=False)

    @timed(timer="union")
    def union(self, lhs: dfa.DFA, rhs: dfa.DFA) -> Any:
        return lhs.union(rhs, minify=False)

    @timed(timer="concat")
    def concat(self, lhs: dfa.DFA, rhs: dfa.DFA) -> dfa.DFA:
        assert False

    @timed(timer="intersection")
    def intersection_all(self, aut_list: list) -> Any:
        result = aut_list[0]
        for aut in aut_list[1:]:
            result = result.intersection(aut, minify=False)
        return result

    @timed(timer="complement")
    def complement(self, lhs: dfa.DFA, alphabet: Any) -> Any:
        return lhs.complement(minify=False)

    @timed(timer="inclusion")
    def inclusion(self, lhs: dfa.DFA, rhs: dfa.DFA) -> Any:
        """TODO: Is this correct? If find this strange"""
        return lhs.issubset(rhs)

    @timed(timer="emptiness")
    def is_empty(self, aut: dfa.DFA) -> bool:
        return aut.isempty()
