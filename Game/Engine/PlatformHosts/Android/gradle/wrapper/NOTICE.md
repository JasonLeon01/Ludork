# Gradle Wrapper / Gradle wrapper 来源说明

The Gradle wrapper is licensed under the Apache License 2.0; the complete text is in [LICENSE.txt](LICENSE.txt). Copyright notices remain in `../../gradlew` and `../../gradlew.bat`. Upstream: [Gradle](https://github.com/gradle/gradle).

Ludork maintains the wrapper launchers and JAR in its Android host template. The files were copied unchanged from `examples/projects/android` in [SFML-ME, tag 310-ME-OH-GLESVER](https://github.com/JasonLeon01/SFML-ME/tree/310-ME-OH-GLESVER/examples/projects/android). Packaging reads only this Ludork template. The adjacent `gradle-wrapper.properties` selects Gradle 9.5.0 independently of that source snapshot.

Gradle wrapper 使用 Apache License 2.0，完整文本见 [LICENSE.txt](LICENSE.txt)。`../../gradlew` 和 `../../gradlew.bat` 保留原始版权声明，上游项目为 [Gradle](https://github.com/gradle/gradle)。

Ludork 在自己的 Android 宿主模板中维护 wrapper 启动脚本与 JAR。三个文件原样取自 SFML-ME 的 `310-ME-OH-GLESVER` 标签下的 `examples/projects/android`，打包时只读取 Ludork 模板。同目录的 `gradle-wrapper.properties` 独立指定 Gradle 9.5.0，不随来源快照改变。

## Imported file SHA-256 / 导入文件 SHA-256

| File / 文件 | SHA-256 |
| --- | --- |
| `../../gradlew` | `63135287117a1e6d12c84580f1f49c61d1ba02218ecd28660605e97f976e7d65` |
| `../../gradlew.bat` | `c46a27c79007746de5922b17abb6230d64ad8b1ba3ad1585ee5c6543c2a9b129` |
| `gradle-wrapper.jar` | `e996d452d2645e70c01c11143ca2d3742734a28da2bf61f25c82bdc288c9e637` |
