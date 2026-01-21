from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

from tree_sitter import Language, Parser
from tree_sitter_cpp import language as cppLanguage


formatTypeMap = {
    "i": "int",
    "l": "int",
    "n": "int",
    "h": "int",
    "H": "int",
    "b": "int",
    "B": "int",
    "k": "int",
    "K": "int",
    "L": "int",
    "f": "float",
    "d": "float",
    "p": "bool",
    "s": "str",
}


def iterNodes(node) -> Iterable:
    stack = [node]
    while stack:
        current = stack.pop()
        yield current
        stack.extend(reversed(current.named_children))


def nodeText(source: bytes, node) -> str:
    return source[node.start_byte : node.end_byte].decode("utf-8")


def unquoteStringLiteral(text: str) -> str:
    text = text.strip()
    quoteIndex = text.find('"')
    if quoteIndex == -1 or not text.endswith('"'):
        return text
    return text[quoteIndex + 1 : -1]


def cleanArgName(text: str) -> str:
    cleaned = text.strip()
    if cleaned.startswith("(") and cleaned.endswith(")"):
        cleaned = cleaned[1:-1].strip()
    while cleaned.startswith("&") or cleaned.startswith("*"):
        cleaned = cleaned[1:].lstrip()
    cleaned = re.sub(r"\s+", " ", cleaned)
    if " " in cleaned:
        cleaned = cleaned.split(" ")[-1]
    cleaned = cleaned.strip()
    if cleaned.endswith(")"):
        cleaned = cleaned[:-1].strip()
    return cleaned


def findFirstNode(node, typeNames: Iterable[str]):
    targets = set(typeNames)
    for sub in iterNodes(node):
        if sub.type in targets:
            return sub
    return None


def isPyMethodDefDeclaration(node, source: bytes) -> bool:
    for sub in iterNodes(node):
        if sub.type == "type_identifier" and nodeText(source, sub) == "PyMethodDef":
            return True
    return False


def findMethodTableInitializer(rootNode, source: bytes):
    for node in iterNodes(rootNode):
        if node.type != "declaration":
            continue
        if not isPyMethodDefDeclaration(node, source):
            continue
        initList = findFirstNode(node, ["initializer_list"])
        if initList:
            return initList
    return None


def parseMethodTable(mainCpp: Path, parser: Parser) -> List[Tuple[str, str]]:
    source = mainCpp.read_bytes()
    tree = parser.parse(source)
    initList = findMethodTableInitializer(tree.root_node, source)
    if not initList:
        return []
    result = []
    for child in initList.named_children:
        pyNameNode = findFirstNode(child, ["string_literal"])
        cNameNode = findFirstNode(child, ["identifier"])
        if not pyNameNode or not cNameNode:
            continue
        cName = nodeText(source, cNameNode)
        if cName in ("NULL", "nullptr"):
            continue
        pyName = unquoteStringLiteral(nodeText(source, pyNameNode))
        if not pyName:
            continue
        result.append((pyName, cName))
    return result


def getFunctionName(node, source: bytes) -> str:
    declarator = node.child_by_field_name("declarator")
    if not declarator:
        return ""
    nameNode = findFirstNode(declarator, ["identifier", "qualified_identifier"])
    if not nameNode:
        return ""
    return nodeText(source, nameNode)


def parseParseTupleArgs(argsNode, source: bytes) -> Tuple[str, List[str]]:
    named = argsNode.named_children
    if len(named) < 2:
        return "", []
    fmtIndex = -1
    fmt = ""
    for i, child in enumerate(named):
        if child.type == "string_literal":
            fmt = unquoteStringLiteral(nodeText(source, child))
            fmtIndex = i
            break
    if fmtIndex == -1:
        return "", []
    argNodes = named[fmtIndex + 1 :]
    names = []
    for argNode in argNodes:
        name = cleanArgName(nodeText(source, argNode))
        if name:
            names.append(name)
    return fmt, names


def parsePyargTuple(funcNode, source: bytes) -> Tuple[str, List[str]]:
    for node in iterNodes(funcNode):
        if node.type != "call_expression":
            continue
        funcNodeName = node.child_by_field_name("function")
        if not funcNodeName:
            continue
        if nodeText(source, funcNodeName) != "PyArg_ParseTuple":
            continue
        argsNode = node.child_by_field_name("arguments")
        if not argsNode:
            return "", []
        return parseParseTupleArgs(argsNode, source)
    return "", []


def extractFunctions(cppFile: Path, parser: Parser) -> Dict[str, Tuple[str, str, List[str]]]:
    source = cppFile.read_bytes()
    tree = parser.parse(source)
    functions: Dict[str, Tuple[str, str, List[str]]] = {}
    for node in iterNodes(tree.root_node):
        if node.type != "function_definition":
            continue
        name = getFunctionName(node, source)
        if not name or not name.startswith("C_"):
            continue
        bodyNode = node.child_by_field_name("body")
        if not bodyNode:
            continue
        body = nodeText(source, bodyNode)
        fmt, argNames = parsePyargTuple(node, source)
        functions[name] = (body, fmt, argNames)
    return functions


def parseFormatTypes(fmt: str, count: int) -> List[str]:
    types: List[str] = []
    i = 0
    while i < len(fmt):
        ch = fmt[i]
        if ch in ("|", ":", ";", " "):
            i += 1
            continue
        if ch == "O":
            types.append("Any")
            if i + 1 < len(fmt) and fmt[i + 1] in ("!", "&"):
                i += 2
                continue
            i += 1
            continue
        if ch in formatTypeMap:
            types.append(formatTypeMap[ch])
            i += 1
            continue
        types.append("Any")
        i += 1
    if len(types) != count:
        types = ["Any"] * count
    return types


def inferReturnType(body: str, funcName: str) -> str:
    if "Py_RETURN_NONE" in body:
        return "None"
    if funcName == "C_GetLightMap":
        return "list[list[float]]"
    if "PyList_New" in body or "FromVectorPyObjToPyList" in body:
        return "list[Any]"
    return "Any"


def buildPyi(methods: List[Tuple[str, str]], funcs: Dict[str, Tuple[str, str, List[str]]]) -> str:
    lines = ["from typing import Any", ""]
    for pyName, cName in methods:
        body, fmt, argNames = funcs.get(cName, ("", "", []))
        types = parseFormatTypes(fmt, len(argNames))
        if not argNames:
            argsSig = "*args: Any"
        else:
            params = [f"{name}: {typ}" for name, typ in zip(argNames, types)]
            argsSig = ", ".join(params)
        ret = inferReturnType(body, cName)
        lines.append(f"def {pyName}({argsSig}) -> {ret}: ...")
    lines.append("")
    return "\n".join(lines)


def findModuleName(targetDir: Path) -> Optional[str]:
    candidates = sorted(list(targetDir.rglob("*.pyd")) + list(targetDir.rglob("*.so")))
    if not candidates:
        return None
    name = candidates[0].name
    return name.split(".")[0]


def resolveTargetDir() -> Path:
    if len(sys.argv) > 1:
        return Path(sys.argv[1]).resolve()
    return Path.cwd().resolve()


def main() -> None:
    targetDir = resolveTargetDir()
    print("Scanning C extension sources...")
    moduleName = findModuleName(targetDir)
    if not moduleName:
        print(f"No .pyd or .so found in {targetDir}")
        raise SystemExit(1)
    parser = Parser()
    parser.language = Language(cppLanguage())
    cppFiles = list(targetDir.glob("src/**/*.cpp")) + [targetDir / "main.cpp"]
    funcs: Dict[str, Tuple[str, str, List[str]]] = {}
    for cppFile in cppFiles:
        if not cppFile.exists():
            continue
        funcs.update(extractFunctions(cppFile, parser))
    mainCpp = targetDir / "main.cpp"
    if not mainCpp.exists():
        print(f"main.cpp not found in {targetDir}")
        raise SystemExit(1)
    methods = parseMethodTable(mainCpp, parser)
    pyi = buildPyi(methods, funcs)
    outPath = targetDir / f"{moduleName}.pyi"
    outPath.write_text(pyi, encoding="utf-8")
    print(f"Generated {outPath}")


if __name__ == "__main__":
    main()
