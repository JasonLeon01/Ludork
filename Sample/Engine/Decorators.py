# -*- encoding: utf-8 -*-

from typing import Type, Union, Tuple, Callable, List
import functools


def TypeAdapter(**typeMap: Union[Tuple[Union[Type, List[Type]], Type], Tuple[Union[Type, List[Type]], Type, Callable]]):
    r"""
    \brief Decorator for adapting parameter types in function calls.

    This decorator allows automatic type conversion of function parameters
    based on a type mapping. It can convert values from an origin type
    to a target type using an optional converter function.

    - typeMap: Keyword arguments mapping parameter names to type tuples.
               Each tuple should be either:
               - (originType, targetType): uses targetType as converter
               - (originType, targetType, converter): uses specified converter
               The originType can be a single Type or a List[Type] for multiple accepted types.

    \return A decorator function that wraps the target function with type adaptation.
    """

    def decorator(func: Callable):
        argNames = func.__code__.co_varnames[: func.__code__.co_argcount]

        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            fullArgs = dict(zip(argNames, args))
            fullArgs.update(kwargs)
            for paramName, types in typeMap.items():
                if not paramName in fullArgs:
                    continue
                if fullArgs[paramName] is None:
                    continue
                converter = None
                if len(types) == 2:
                    originType, targetType = types
                    converter = targetType
                elif len(types) == 3:
                    originType, targetType, converter = types
                else:
                    raise ValueError(f"Error: Parameter {paramName} must have 2 or 3 types.")
                value = fullArgs[paramName]
                if not isinstance(value, targetType):
                    isOriginType = False
                    if isinstance(originType, list):
                        for t in originType:
                            if isinstance(value, t):
                                isOriginType = True
                                break
                    else:
                        isOriginType = isinstance(value, originType)
                    if isOriginType:
                        if isinstance(value, list) or isinstance(value, tuple):
                            fullArgs[paramName] = converter(*value)
                        else:
                            fullArgs[paramName] = converter(value)
                    else:
                        raise TypeError(f"Error: Parameter {paramName} must be of type {originType} or {targetType}")
            return func(**fullArgs)

        return wrapper

    return decorator


def Meta(**kwargs):
    r"""
    \brief Decorator for attaching metadata to a function.

    This decorator adds a _meta dictionary to the decorated function,
    which can be used to store arbitrary metadata about the function.

    - kwargs: Key-value pairs to be stored as metadata.

    \return A decorator function that attaches metadata to the target function.
    """

    def decorator(func):
        func._meta = kwargs
        return func

    return decorator


def ExecSplit(**kwargs):
    r"""
    \brief Decorator for marking execution split points in node graph.

    This decorator adds _execSplits and _refLocal attributes to the decorated function,
    which are used for controlling execution flow in the visual scripting system.

    - kwargs: Key-value pairs defining execution split behavior.

    \return A decorator function that attaches execution split metadata.
    """

    def decorator(func):
        func._execSplits = kwargs
        if not hasattr(func, "_refLocal"):
            func._refLocal = {}
        return func

    return decorator


def Latent(**kwargs):
    r"""
    \brief Decorator for marking functions as latent (async) operations.

    This decorator adds _latents and _refLocal attributes to the decorated function,
    which are used for handling asynchronous operations in the visual scripting system.

    - kwargs: Key-value pairs defining latent behavior.

    \return A decorator function that attaches latent metadata.
    """

    def decorator(func):
        func._latents = kwargs
        if not hasattr(func, "_refLocal"):
            func._refLocal = {}
        return func

    return decorator


def ReturnType(**kwargs):
    r"""
    \brief Decorator for specifying return types of a function.

    This decorator adds _returnTypes and _refLocal attributes to the decorated function,
    which are used for type checking and documentation in the visual scripting system.

    - kwargs: Key-value pairs mapping parameter names to their types.

    \return A decorator function that attaches return type metadata.
    """

    def decorator(func):
        func._returnTypes = kwargs
        if not hasattr(func, "_refLocal"):
            func._refLocal = {}
        return func

    return decorator


def InvalidVars(*args):
    r"""
    \brief Class decorator for marking invalid variables.

    This decorator adds _invalidVars attribute to the decorated class,
    which specifies variables that should not be serialized or displayed.

    - args: Variable names that should be considered invalid.

    \return A class decorator that attaches invalid variables metadata.
    """

    def decorator(cls):
        cls._invalidVars = args
        return cls

    return decorator


def PathVars(*args):
    r"""
    \brief Class decorator for marking path-type variables.

    This decorator adds _pathVars attribute to the decorated class,
    which specifies variables that represent file system paths and may
    require special handling (e.g., relative path resolution).

    - args: Variable names that represent paths.

    \return A class decorator that attaches path variables metadata.
    """

    def decorator(cls):
        cls._pathVars = args
        return cls

    return decorator


def RectRangeVars(**kwargs):
    r"""
    \brief Class decorator for marking rectangle range variables.

    This decorator adds _rectRangeVars attribute to the decorated class,
    which specifies variables that represent rectangular ranges and may
    require special UI handling (e.g., range selection widgets).

    - kwargs: Key-value pairs mapping variable names to their range specifications.

    \return A class decorator that attaches rectangle range variables metadata.
    """

    def decorator(cls):
        cls._rectRangeVars = kwargs
        return cls

    return decorator


def RegisterEvent(func=None):
    r"""
    \brief Decorator for registering a function as an event handler.

    This decorator adds _eventSignature attribute to the decorated function,
    which marks it as an event handler in the visual scripting system.
    Can be used with or without parentheses.

    - func: Optional function to decorate directly (allows usage without parentheses).

    \return A decorator function that attaches event signature metadata.
    """

    def decorator(f):
        f._eventSignature = True
        return f

    if func is None:
        return decorator
    return decorator(func)


__all__ = [
    "TypeAdapter",
    "Meta",
    "ExecSplit",
    "Latent",
    "ReturnType",
    "InvalidVars",
    "PathVars",
    "RectRangeVars",
    "RegisterEvent",
]
