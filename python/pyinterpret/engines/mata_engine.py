from typing import Any
import libmata.nfa.nfa as libmata

import pyinterpret.engines.base as engine_base
from pyinterpret.utils import timed


class MataEngine(engine_base.Engine):
    @timed(timer="union")
    def union(self, lhs: Any, rhs: Any) -> Any:
        return libmata.union(lhs, rhs)

    @timed(timer="intersection")
    def intersection_all(self, aut_list: list) -> Any:
        result = aut_list[0]
        for aut in aut_list[1:]:
            result = libmata.intersection(result, aut)
        return result

    @timed(timer="complement")
    def complement(self, lhs: Any, alphabet: Any) -> Any:
        return libmata.complement(lhs, alphabet)

    @timed(timer="inclusion")
    def inclusion(self, lhs: Any, rhs: Any) -> Any:
        return libmata.is_included(lhs, rhs)

    def load_db(self, db: dict) -> Any:
        return db

    @timed(timer="intersection")
    def intersection(self, lhs: Any, rhs: Any) -> Any:
        return libmata.intersection(lhs, rhs)

    @timed(timer="emptiness")
    def is_empty(self, aut: Any) -> bool:
        return libmata.is_lang_empty(aut)

