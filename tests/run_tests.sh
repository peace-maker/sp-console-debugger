#!/bin/bash
set -eo pipefail

save_output=0

cwd=$(dirname $(readlink -f "$0"))

# $cwd/mock/gamedir/addons/sourcemod/scripting/spcomp ./debugger_test.sp -odebugger_test_latest.smx

cd ..
python3 configure.py --sm-path "$cwd/mock/sourcemod" --targets x86,x86_64
ambuild objdir

cd "$cwd/mock/hl2sdk-mock"
bash build_gamedir.sh "$cwd/mock/gamedir" "$cwd/../objdir/package"

function test_output {
    cd "$cwd/mock/hl2sdk-mock"
    local pluginname="$1"
    local linenumber="$2"
    ln -sf "$cwd/$pluginname.smx" "$cwd/mock/gamedir/addons/sourcemod/plugins/$pluginname.smx"
    local output=$(./objdir/dist/x86_64/srcds -game_dir "$cwd/mock/gamedir" +map de_thunder -run-ticks 20 <<- EOF
sm debug start $pluginname.smx
sm debug bp $pluginname.smx add $linenumber
bp
continue
print *
quit
quit
EOF
)
    cd ../..

    debug_output=$(sed -n '/START DEBUG/,/STOP DEBUG/p' <<< "$output")
    if [ "$save_output" == 1 ]; then
        echo "$debug_output" > "$pluginname.out"
    fi
    expected_output=$(<"$pluginname.out")
    if [ "$debug_output" != "$expected_output" ]; then
        echo "$output"
        rm "$cwd/mock/gamedir/addons/sourcemod/plugins/$pluginname.smx"
        echo "Output does not match expected output"
        diff -u --color=always <(echo "$expected_output") <(echo "$debug_output")
        exit 1
    fi

    rm "$cwd/mock/gamedir/addons/sourcemod/plugins/$pluginname.smx"
    echo "$pluginname: Test passed"
}

test_output debugger_test_latest 96
test_output debugger_test_1.7 90
