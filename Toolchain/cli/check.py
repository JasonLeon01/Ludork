from .dispatch import dispatch


def main(arguments: list[str] | None = None) -> int:
    return dispatch('check', arguments)


if __name__ == "__main__":
    raise SystemExit(main())
