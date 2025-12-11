Gamebar Autodeafen

Gamebar Autodeafen is a lightweight Geode mod for Geometry Dash (Windows) that automatically mutes Xbox Game Bar party chat when the player reaches a user-defined percentage in a level. It unmutes on pause, restart, death, completion, or when exiting the level. The system is designed to be simple, stable, and performance-safe.

Features

Configurable deafen and undeafen percentages

Settings available in the pause menu inside levels

Full Geode settings integration

No performance impact

Compatible with Xbox Game Bar party chat

Automatically handles pause, restart, and attempt flow correctly

How It Works

When the player reaches the "Deafen Percent", the mod mutes XboxPartyChatHost.exe.
When the player reaches the "Undeafen Percent", the mod unmutes.
Pausing the game always unmutes. Restarting unmutes and re-applies mute if the player is already past the threshold.
Dying or exiting unmutes fully.

Configuration

In the pause menu:

Toggle Autodeafen for the current level

Adjust deafen/undeafen percentages

See current mute status

In Geode mod settings:

Enabled by Default

Enabled in Practice Mode

Deafen Percent

Undeafen Percent

Additional Debug Logging

Installation

Download the latest .geode file from the Releases page.

Install it through Geometry Dash > Geode > Mods > Install From File.

Manual installation:
Place the .geode file into:
%localappdata%\GeometryDash\geode\mods

Requirements

Geometry Dash 2.2074 (Windows)

Geode Loader 4.0 or later

Xbox Game Bar installed

Development

To build:
geode build

The built mod will appear inside the build folder.

Folder Structure:
src/main.cpp
resources/icon.png
mod.json
about.md
README.txt

Credits

Original AutoDeafen concept by Lynxdeer
Rewritten and extended by itsobrie
Development assistance provided by ChatGPT
Custom icons created specifically for this project

License

This project uses the MIT License. You may use, modify, and redistribute the code as long as attribution is retained.

Links

GitHub Repository: (insert link here)
Geode Mod Page: (insert link when published)