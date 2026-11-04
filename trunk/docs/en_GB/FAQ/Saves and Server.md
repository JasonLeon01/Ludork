# Saves and Server

## Why do changed game-variable defaults not affect an existing save?

Saved variable declarations provide initial values for new games. Loading a slot replaces the new instance's variable table with the saved table; it does not merge added declarations or migrate existing values. Apply changes to a new game or provide your own save migration. See [Game variables](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#game-variables>).

## Do I need Ludork Server, and does it provide multiplayer or cloud saves?

Ludork Server is an optional Node.js service with server Lua logic, explicit snapshot proxies, void RPCs and online events. Gameplay chooses when to fetch or refresh data and which business actions to send. It does not automatically synchronize multiplayer state or upload, download or replace local saves. Configuring client integration requires C++ Source. See [Simple Server](<../Simple Server/Deployment and Editor Settings.md#simple-server-deployment-and-editor-settings>) and [Configuration](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#configuration>).
