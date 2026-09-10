# Plugins
These plugins are quite primitive however, the idea is that you can load or unload and run arbitrary code on the 3DS whenever without needing any libraries just 2 technically optional header fils containing types and API data.
It uses the [Debug3dsServerConsole](Debug3dsServerConsole/build/Debug3dsServerConsole.dll) files in a console. 
It can be ran with the exe either by just using the exe on windows or using `dotnet Debug3dsServerConsole.dll` on Linux.

A plugin can easily be made with little work, the [example plugin files](pluginTest) should be enough to easily create a plugin.
Plugins need to be compiled extremely specifically and put into a custom file type which is why a [run.py](run.py) file is included (I did want to implement a ful elf file parser).
The file type is simply raw data and most of the elf data striped away, this keeps the file small and easy to prase.
Because of this some functions added by the compiler may not work. `memset`, `memcpy` and `memcmp` are the few exceptions as they have been implanted but just about all others from external headers will not work.

Do note however that there is no safety net for these plugins, they run at the same level as the firmware does, no abstraction or layers.
If you write bad code the 3DS will crash.