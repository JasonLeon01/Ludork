# -*- encoding: utf-8 -*-


def already_packed() -> bool:
    result = False
    try:
        result = __compiled__ is not None
    except Exception:
        pass
    if not result:
        try:
            result = __nuitka_binary_dir is not None
        except Exception:
            pass
    return result


def get_title() -> str:
    import EditorStatus

    titles = [EditorStatus.APP_NAME]
    result = ""
    try:
        import Data

        if EditorStatus.PROJ_PATH:
            cfg = Data.GameData.systemConfigData.get("System")
            if isinstance(cfg, dict):
                t = cfg.get("title")
                title = t.get("value") if isinstance(t, dict) else t
                if isinstance(title, str) and title.strip():
                    titles.append(title.strip())
                    result = " - ".join(titles)
                    mods = (
                        bool(getattr(Data.GameData, "modifiedMaps", []))
                        or bool(getattr(Data.GameData, "modifiedSystemConfigs", []))
                        or bool(getattr(Data.GameData, "modifiedTilesets", []))
                    )
                    if mods:
                        result += " *"
    except Exception as e:
        print(f"Error while getting title: {e}")
        result = " - ".join(titles)
    return result
