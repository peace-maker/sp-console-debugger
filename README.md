# SourcePawn Console Debugger
A console debugger within a local "shell" to debug SourceMod plugins.
Based on pawndbg, the Pawn debugger, by ITB CompuPhase.

It adds a new `debug` option to the `sm` root console menu:
```
sm debug
SourceMod Debug Menu:
    start            - Start debugging a plugin
    next             - Start debugging the plugin which is loaded next
    bp               - Handle breakpoints in a plugin

sm debug start
[SM] Usage: sm debug start <#|file>

sm debug bp
[SM] Usage: sm debug bp <#|file> <option>
    list             - List breakpoints
    add              - Add a breakpoint
    remove           - Remove a breakpoint

sm debug bp plugin add
[SM] Usage: sm debug bp <#|file> add <file:line | file:function>
```

## Shell usage
Basic commands as listed by the `?` command:
```
At the prompt, you can type debug commands. For example, the word "step" is a
command to execute a single line in the source code. The commands that you will
use most frequently may be abbreviated to a single letter: instead of the full
word "step", you can also type the letter "s" followed by the enter key.

Available commands:
        backtrace       display the stack trace
        break   set breakpoint at line number or function name
        cbreak  remove breakpoint
        cwatch  remove a "watchpoint"
        continue        run program (until breakpoint)
        files   list all files that this program is composed off
        frame   select a frame from the back trace to operate on
        funcs   display functions
        next    run until next line, step over functions
        position        show current file and line
        print   display the value of a variable, list variables
        quit    exit debugger
        set     set a variable to a value
        step    single step, step into functions
        x       eXamine plugin memory: x/FMT ADDRESS
        watch   set a "watchpoint" on a variable

        Use "? <command name>" to view more information on a command
```

## Installation

Currently, a [modified SourcePawn VM](https://github.com/peace-maker/sourcepawn/tree/debug_api_symbols) is required which exposes information about the debug symbols of a plugin. To use this, you have to build the sourcepawn VM with the required changes.

To build sourcemod with the modified SourcePawn VM, replace the sourcepawn submodule with the forked one and follow the [normal build instructions](https://wiki.alliedmods.net/Building_SourceMod):
```
git clone --recurse-submodules https://github.com/alliedmodders/sourcemod sourcemod_debugger
# Switch to the debug symbol modification of SourcePawn
cd sourcemod_debugger/sourcepawn
git remote add peace-maker https://github.com/peace-maker/sourcepawn
git fetch peace-maker
git switch debug_api_symbols
cd ..
python configure.py --sdks css --enable-optimize --no-mysql
ambuild objdir
# install sourcemod from objdir/package
```

To build the extension:
```
git clone https://github.com/peace-maker/sp-console-debugger
cd sp-console-debugger
python configure.py --enable-optimize --sm-path /path/to/sourcemod_debugger
ambuild objdir
# install extension from objdir/package. make sure to copy the .autoload file too.
```
