# Ludork

Ludork 是一个 2D RPG 工具包，基于 Python、PyQt5、SFML/PySF 和 pybind11 原生扩展构建。它提供一个用于创建项目数据的编辑器，以及直接使用相同数据的 Python 游戏运行时。

仓库包含工具包应用、原生扩展构建链，以及一个完整的 `Sample/` 示例项目，可用作新游戏的模板。

## 包含内容

- `main.py` 和 `LoadEditor.py`：应用入口与编辑器启动。
- `EditorGlobal/`、`Widgets/`、`NodeGraph/`、`Utils/`：编辑器端应用代码。
- `Sample/`：可运行的 RPG 示例项目，包含游戏脚本、数据、资源、存档支持和运行时依赖。
- `Sample/Engine/`、`Sample/Global/`、`Sample/Source/`：游戏运行时与示例玩法代码。
- `C_Extensions/`：pybind11 和 SFML 原生扩展源码。
- `docs/en_GB/` 和 `docs/zh_CN/`：编辑器工作流与运行时脚本手册。

## 环境要求

- Python 3.12
- CMake 和 C++ 工具链（用于构建原生扩展）
- Windows PowerShell，或 macOS/Linux 上的标准 Shell 工具

## 快速开始

初始化设置：

```bat
.\init.bat
```

```bash
./init.sh
```

之后启动编辑器：

```bat
.\run.bat
```

```bash
./run.sh
```

直接运行示例游戏：

```bash
cd Sample
python Entry.py
```

手动构建原生扩展：

```bash
python C_Extensions/build.py
```

## 文档

手册先说明编辑器使用，再说明运行时和脚本参考：

- 英文：`docs/en_GB/`
- 中文：`docs/zh_CN/`

从 `02`-`06` 章阅读编辑器工作流，包括项目设置、素材、地图、蓝图、数据、本地化、节点和测试。`07`-`17` 章用于查阅运行时结构、API、脚本、配置和打包。

## 许可证

参见 [LICENSE.md](LICENSE.md)。
