# Saves and Server

## Why do changed game-variable defaults not affect an existing save?

Saved variable declarations provide initial values for new games. Loading a slot replaces the new instance's variable table with the saved table; it does not merge added declarations or migrate existing values. Apply changes to a new game or provide your own save migration. See [Game variables](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#game-variables>).

## Do I need Ludork Server, and does it provide multiplayer or cloud saves?

Ludork Server is an optional HTTP store for account data dictionaries. It provides neither multiplayer synchronisation nor RPC and does not automatically upload, download or replace local saves. Gameplay chooses which fields to read and write. Configuring client integration requires C++ Source. See [Simple Server](<../Simple Server/Deployment and Editor Settings.md#simple-server-deployment-and-editor-settings>) and [Configuration](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#configuration>).
