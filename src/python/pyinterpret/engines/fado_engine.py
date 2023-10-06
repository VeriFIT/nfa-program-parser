from typing import Any

import pyinterpret.engines.base as engine_base
from pyinterpret.utils import timed
import FAdo.fa as fa


class FadoEngine(engine_base.Engine):
    @timed(timer='trim')
    def trim(self, lhs: Any):
        return lhs.trim()

    @timed(timer="determinization")
    def determinize(self, lhs):
        return lhs.toDFA()

    def on_demand_determinize_all(self, automata: list[fa.DFA | fa.NFA]):
        if any(isinstance(aut, fa.DFA) for aut in automata):
            return [
                self.determinize(aut) if isinstance(aut, fa.NFA) else aut for aut in automata
            ]
        else:
            return automata

    def on_demand_determinize(self, lhs: fa.DFA | fa.NFA, rhs: fa.DFA | fa.NFA):
        lhs_is_nfa = isinstance(lhs, fa.NFA)
        rhs_is_nfa = isinstance(rhs, fa.NFA)
        if not lhs_is_nfa and rhs_is_nfa:
            return lhs, self.determinize(rhs)
        elif lhs_is_nfa and not rhs_is_nfa:
            return self.determinize(lhs), rhs
        elif not lhs_is_nfa and not rhs_is_nfa:
            return self.determinize(lhs), self.determinize(rhs)
        else:
            return lhs, rhs

    def force_determinize(self, lhs: fa.DFA | fa.NFA):
        if isinstance(lhs, fa.NFA):
            return self.determinize(lhs)
        return lhs

    @timed(timer="conversion")
    def convert_db(self, db: dict, _) -> Any:
        sigma = set()
        for token, aut in db.items():
            nfa = fa.NFA()
            for init in aut.initial_states:
                nfa.addInitial(nfa.stateIndex(init, True))
            for fin in aut.final_states:
                nfa.addFinal(nfa.stateIndex(fin, True))
            for trans in aut.iterate():
                sigma.add(trans.symbol)
                nfa.addTransition(
                    nfa.stateIndex(trans.source, True),
                    trans.symbol,
                    nfa.stateIndex(trans.target, True)
                )
            db[token] = nfa
        for aut in db.values():
            aut.setSigma(list(sigma))
        return db

    @timed(timer="intersection")
    def intersection(self, lhs: fa.DFA, rhs: fa.DFA) -> Any:
        lhs, rhs = self.on_demand_determinize(lhs, rhs)
        return lhs & rhs

    @timed(timer="union")
    def union(self, lhs: fa.DFA, rhs: fa.DFA) -> Any:
        lhs, rhs = self.on_demand_determinize(lhs, rhs)
        return lhs | rhs

    @timed(timer="concat")
    def concat(self, lhs: fa.DFA, rhs: fa.DFA) -> fa.DFA:
        lhs, rhs = self.on_demand_determinize(lhs, rhs)
        return lhs.concat(rhs)

    @timed(timer="intersection")
    def intersection_all(self, aut_list: list[fa.DFA]) -> Any:
        aut_list = self.on_demand_determinize_all(aut_list)
        result = aut_list[0]
        for aut in aut_list[1:]:
            result = result.conjunction(aut)
        return result

    @timed(timer="union")
    def union_all(self, aut_list: list[fa.DFA]) -> Any:
        aut_list = self.on_demand_determinize_all(aut_list)
        result = aut_list[0]
        for aut in aut_list[1:]:
            result = result.disjunction(aut)
        return result

    @timed(timer="complement")
    def complement(self, lhs: fa.DFA, alphabet: Any) -> Any:
        lhs = self.force_determinize(lhs)
        return ~lhs

    @timed(timer="inclusion")
    def inclusion(self, lhs: fa.DFA, rhs: fa.DFA) -> Any:
        """Warning: naive implementation"""
        lhs, rhs = self.on_demand_determinize(lhs, rhs)
        result = lhs & ~rhs
        return result.emptyP()

    @timed(timer="emptiness")
    def is_empty(self, aut: fa.DFA) -> bool:
        return aut.emptyP()
