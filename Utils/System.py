# -*- encoding: utf-8 -*-


def already_packed() -> bool:
    result = False
    try:
        result = __compiled__ is not None
    except Exception as e:
        pass
    if not result:
        try:
            result = __nuitka_binary_dir is not None
        except Exception as e:
            pass
    return result
