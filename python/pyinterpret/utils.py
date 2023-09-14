import time


def timed(*iarg, **ikwargs):
    """Decorator function for timing

    :param func: wrapped function
    :param kwargs: additional arguments
    :return:
    """
    def wrapper(func):
        def inner_wrapper(*args, **kwargs):
            tic = time.perf_counter()
            result = func(*args, **kwargs)
            toc = time.perf_counter()
            print(f"{ikwargs.get('timer', 'unknown')}: {toc - tic:0.4f}")
            return result
        return inner_wrapper
    return wrapper
