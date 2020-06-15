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
    clear            - Remove a breakpoint

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
        B(ack)T(race)   display the stack trace
        B(reak)         set breakpoint at line number or function name
        CB(reak)        remove breakpoint
        CW(atch)        remove a "watchpoint"
        D(isp)          display the value of a variable, list variables
        FILES           list all files that this program is composed off
        FUNCS           display functions
        C(ontinue)      run program (until breakpoint)
        N(ext)          Run until next line, step over functions
        POS             Show current file and line
        QUIT            exit debugger
        SET             Set a variable to a value
        S(tep)          single step, step into functions
        W(atch)         set a "watchpoint" on a variable
        X               eXamine plugin memory: x/FMT ADDRESS

        Use "? <command name>" to view more information on a command
```
