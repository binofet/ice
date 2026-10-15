# Modern engine port status

This game is retained source, art and its existing physics snapshot. The modern root
build does not compile it: the prior fixed-function/device/input/UI APIs were removed.
Historical IDE files are not maintained build definitions. See [EXAMPLES.md](../../EXAMPLES.md)
for the shared renderer/input/physics port sequence. Use an engine-level optional
physics integration when restoring this game, rather than reviving obsolete APIs.
