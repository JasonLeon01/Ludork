# -*- encoding: utf-8 -*-
# Additional builtins injected at runtime by Engine/__init__.py (for Sample runtime)
# and by editor (for ELOC).

from typing import Any, Callable, TypeVar

_T = TypeVar("_T")
_F = TypeVar("_F", bound=Callable[..., Any])

# Editor builtin: localisation for editor UI
def ELOC(key: str) -> str: ...

# Sample runtime builtins: injected by Engine/__init__.py
def LOC(key: str) -> str: ...
def TypeAdapter(**kwargs: Any) -> Callable[[_F], _F]:
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
    ...
def Meta(**kwargs: Any) -> Callable[[_F], _F]:
    r"""
    \brief Decorator for attaching metadata to a function.

    This decorator adds a _meta dictionary to the decorated function,
    which can be used to store arbitrary metadata about the function.

    - kwargs: Key-value pairs to be stored as metadata.

    \return A decorator function that attaches metadata to the target function.
    """
    ...
def ExecSplit(**kwargs: Any) -> Callable[[_F], _F]:
    r"""
    \brief Decorator for marking execution split points in node graph.

    This decorator adds _execSplits and _refLocal attributes to the decorated function,
    which are used for controlling execution flow in the visual scripting system.

    - kwargs: Key-value pairs defining execution split behavior.

    \return A decorator function that attaches execution split metadata.
    """
    ...
def Latent(**kwargs: Any) -> Callable[[_F], _F]:
    r"""
    \brief Decorator for marking functions as latent (async) operations.

    This decorator adds _latents and _refLocal attributes to the decorated function,
    which are used for handling asynchronous operations in the visual scripting system.

    - kwargs: Key-value pairs defining latent behavior.

    \return A decorator function that attaches latent metadata.
    """
    ...
def ReturnType(**kwargs: Any) -> Callable[[_F], _F]:
    r"""
    \brief Decorator for specifying return types of a function.

    This decorator adds _returnTypes and _refLocal attributes to the decorated function,
    which are used for type checking and documentation in the visual scripting system.

    - kwargs: Key-value pairs mapping parameter names to their types.

    \return A decorator function that attaches return type metadata.
    """
    ...
def InvalidVars(*args: str) -> Callable[[type[_T]], type[_T]]:
    r"""
    \brief Class decorator for marking invalid variables.

    This decorator adds _invalidVars attribute to the decorated class,
    which specifies variables that should not be serialized or displayed.

    - args: Variable names that should be considered invalid.

    \return A class decorator that attaches invalid variables metadata.
    """
    ...
def PathVars(*args: str) -> Callable[[type[_T]], type[_T]]:
    r"""
    \brief Class decorator for marking path-type variables.

    This decorator adds _pathVars attribute to the decorated class,
    which specifies variables that represent file system paths and may
    require special handling (e.g., relative path resolution).

    - args: Variable names that represent paths.

    \return A class decorator that attaches path variables metadata.
    """
    ...
def RectRangeVars(**kwargs: Any) -> Callable[[type[_T]], type[_T]]:
    r"""
    \brief Class decorator for marking rectangle range variables.

    This decorator adds _rectRangeVars attribute to the decorated class,
    which specifies variables that represent rectangular ranges and may
    require special UI handling (e.g., range selection widgets).

    - kwargs: Key-value pairs mapping variable names to their range specifications.

    \return A class decorator that attaches rectangle range variables metadata.
    """
    ...
def RegisterEvent(func: _F = ...) -> _F:
    r"""
    \brief Decorator for registering a function as an event handler.

    This decorator adds _eventSignature attribute to the decorated function,
    which marks it as an event handler in the visual scripting system.
    Can be used with or without parentheses.

    - func: Optional function to decorate directly (allows usage without parentheses).

    \return A decorator function that attaches event signature metadata.
    """
    ...
def Cast[T](targetType: type[T], value: Any) -> T:
    r"""
    \brief Cast a value to the specified type with a runtime type assertion on targetType.

    - \param targetType The target type to cast to.
    - \param value The value to cast.
    - \return The value, typed as targetType.
    """
    ...
def AssertType[T](obj: Any, type_: type[T]) -> None:
    r"""
    \brief Recursively assert that obj conforms to the given type annotation.

    Supports concrete types, generic aliases (list, dict, tuple, set, frozenset),
    and union types (X | Y / Union[X, Y] / Optional[X]).

    - \param obj The object to validate.
    - \param type_ The type annotation to validate against.
    """
    ...
