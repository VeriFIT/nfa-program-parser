from typing import Any

import pyinterpret.engines.base as engine_base
from pyinterpret.utils import timed
import FAdo.fa as fa


class FadoEngine(engine_base.Engine):
    @timed(timer="conversion")
    def convert_db(self, db: dict, alphabet) -> Any:
        for token, aut in db.items():
            nfa = fa.NFA()
            for init in aut.initial_states:
                nfa.addInitial(nfa.stateIndex(init, True))
            for fin in aut.final_states:
                nfa.addInitial(nfa.stateIndex(fin, True))
            for trans in aut.iterate():
                nfa.addTransition(
                    nfa.stateIndex(trans.source, True),
                    trans.symbol,
                    nfa.stateIndex(trans.target, True)
                )
            db[token] = nfa.toDFA()
        return db

    @timed(timer="intersection")
    def intersection(self, lhs: fa.DFA, rhs: fa.DFA) -> Any:
        return lhs & rhs

    @timed(timer="union")
    def union(self, lhs: fa.DFA, rhs: fa.DFA) -> Any:
        return lhs | rhs

    @timed(timer="intersection")
    def intersection_all(self, aut_list: list[fa.DFA]) -> Any:
        result = aut_list[0]
        for aut in aut_list[1:]:
            result = result.conjunction(aut)
        return result

    @timed(timer="complement")
    def complement(self, lhs: fa.DFA, alphabet: Any) -> Any:
        return ~lhs

    @timed(timer="inclusion")
    def inclusion(self, lhs: fa.DFA, rhs: fa.DFA) -> Any:
        """Warning: naive implementation"""
        result = lhs & ~rhs
        return result.emptyP()

    @timed(timer="emptiness")
    def is_empty(self, aut: fa.DFA) -> bool:
        return aut.emptyP()
