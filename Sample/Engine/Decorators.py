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
    \brief Decorator for attaching metadata to a function or class.

    This decorator adds a _meta dictionary to the decorated object,
    which can be used to store arbitrary metadata about that object.
    Editor-recognised keys include:
    - DisplayName: node/function display name. Usually a string expression
      such as `LOC("SAVE_GAME")`; the editor evaluates it when building node
      titles and the function picker search cache.
    - DisplayDesc: node/function tooltip or description. Usually a string
      expression such as `LOC("SAVE_GAME_DESC")`.
    - DropBox: node parameter combo-box options in the form
      `{"paramName": ["optionA", "optionB"]}`.
    - Rely: editor-side edit dependencies in the form
      `{"target": ["source", expectedValue]}` or
      `{"target": {"source": "source", "value": expectedValue}}`.
      The dict form also accepts `key` or `var` instead of `source`.
    - PathVars: path fields or node parameters in the form
      `[("texturePath", "Characters")]`; string items such as
      `["texturePath"]` default to the `Characters` assets directory.
    - ColourVars: colour fields or parameters in the form
      `["lightColour"]`. Values store RGBA tuples but display through a colour
      swatch editor.
    - Vector2Vars / Vector2fVars / Vector2iVars / Vector2uVars and
      PairVars / PairFloatVars / PairIntVars: two-component fields or node
      parameters displayed with x/y numeric editors.
    - Vector3Vars / Vector3fVars / Vector3iVars / Vector3uVars:
      three-component fields or node parameters displayed with x/y/z numeric
      editors.
    - MoveRouteVars: movement-route node parameters displayed with a map
      reference route editor. Values are stored as relative `(dx, dy)` steps.
    - Transfer: map coordinate node parameters displayed with a map tile
      picker. Format: `[("posVarName", "mapVarName")]`.
    - GeneralDataVars: General Data member or animation-key fields and node
      parameters in the form `[("itemID", "Item"), ("animKey", "ANIMATION")]`.
      Values remain plain strings; the editor displays a data-backed combo box.
    - InvalidVars: class variables hidden from the blueprint attribute editor.
      This is the Meta form of `@InvalidVars(...)`.
    - RectRangeVars: rectangle fields edited with an image range selector.
      This is the Meta form of `@RectRangeVars(...)`.

    - kwargs: Key-value pairs to be stored as metadata.

    \return A decorator function that attaches metadata to the target function.
    """

    def decorator(func):
        existing = getattr(func, "_meta", None)
        meta = dict(existing) if isinstance(existing, dict) else {}
        meta.update(kwargs)
        func._meta = meta
        invalidVars = kwargs.get("InvalidVars")
        if isinstance(invalidVars, str):
            func._invalidVars = (invalidVars,)
        elif isinstance(invalidVars, (list, tuple, set)):
            func._invalidVars = tuple(name for name in invalidVars if isinstance(name, str))
        rectRangeVars = kwargs.get("RectRangeVars")
        if isinstance(rectRangeVars, dict):
            func._rectRangeVars = dict(rectRangeVars)
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
        f._execSplits = {"default": (None,)}
        if not hasattr(f, "_refLocal"):
            f._refLocal = {}
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
    "RectRangeVars",
    "RegisterEvent",
]
