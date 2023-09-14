import abc
import sys
import inspect
from typing import Any


class EngineMeta(abc.ABCMeta):
    registry: dict[str, 'Engine'] = {}

    def __new__(cls, name, bases, namespace):
        new_cls = super().__new__(cls, name, bases, namespace)
        if not inspect.isabstract(new_cls):
            cls.registry[new_cls.__name__] = new_cls
        return new_cls


class Engine(abc.ABC, metaclass=EngineMeta):
    # Property holding create automata mapping their name to implementation
    automata_db: dict

    @abc.abstractmethod
    def convert_db(self, db: dict, alphabet) -> Any:
        """Processes loaded db into supported format

        :param db: automata database
        :return: automata database with converted automata
        """
        pass

    @abc.abstractmethod
    def intersection(self, lhs: Any, rhs: Any) -> Any:
        """Implementation of `aut3 = (inter aut1 aut2)` instruction

        Makes intersection of aut1 and aut2 creating automaton in process

        :param lhs: left-hand side of intersection
        :param rhs: right-hand side of intersection
        :return: intersection of two automata
        """
        pass

    @abc.abstractmethod
    def union(self, lhs: Any, rhs: Any) -> Any:
        """Implementation of `aut3 = (union aut1 aut2)` instruction

        Makes union of aut1 and aut2 creating automaton in process

        :param lhs: left-hand side of union
        :param rhs: right-hand side of union
        :return: union of two automata
        """
        pass


    @abc.abstractmethod
    def intersection_all(self, aut_list: list) -> Any:
        """Implementation of `aut3 = (interall)` instruction

        Makes union of aut1 and aut2 creating automaton in process

        :param aut_list: list of automata
        :return: intersection of all automata in list
        """
        pass

    @abc.abstractmethod
    def complement(self, lhs: Any, alphabet: Any) -> Any:
        """Implementation of `compl aut1` instruction

        Makes complement of aut1

        :param lhs: left-hand side of complement
        :param alhpabet: alphabet for complement
        :return: complement of two automata
        """
        pass

    @abc.abstractmethod
    def inclusion(self, lhs: Any, rhs: Any) -> Any:
        """Implementation of `incl aut1 aut2` instruction

        Makes inclusion of aut1 and aut2

        :param lhs: left-hand side of inclusion
        :param rhs: right-hand side of inclusion
        :return: inclusion of two automata
        """
        pass

    @abc.abstractmethod
    def is_empty(self, aut: Any) -> bool:
        """Implementation of `is_empty aut3` instruction

        Tests if automaton is empty

        :param aut: tested automaton
        :return: true if automaton is empty
        """
        pass

    @classmethod
    def get_engine(cls, engine_name):
        if engine_name in EngineMeta.registry.keys():
            return EngineMeta.registry[engine_name]()
        else:
            print(f"{engine_name} does not exist")
            sys.exit(1)

