#!/bin/bash

cwd=$(dirname $(readlink -f $0))
mkdir -p "$cwd/mock"
cd mock

function checkout() {
    local name=$1
    local repo=$2
    local branch=$3
    if [ ! -d "$name" ]; then
        git clone --recursive $repo -b $branch $name
    else
        cd $name
        git checkout $branch
        git pull origin $branch
        cd ..
    fi
}

checkout "hl2sdk-mock" "https://github.com/alliedmodders/hl2sdk-mock" "master"
checkout "sourcemod" "https://github.com/alliedmodders/sourcemod" "master"
checkout "metamod-source" "https://github.com/alliedmodders/metamod-source" "master"
pip install --upgrade git+https://github.com/alliedmodders/ambuild.git

cd sourcemod/sourcepawn
git remote add peace https://github.com/peace-maker/sourcepawn
git fetch peace
git checkout debug_api_symbols
git pull peace debug_api_symbols
cd ../..

cd hl2sdk-mock
python3 configure.py --targets=x86,x86_64
ambuild objdir

cd ../metamod-source
python3 configure.py --sdks=mock --targets=x86,x86_64
ambuild objdir

cd ../sourcemod
python3 configure.py --sdks=mock --targets=x86,x86_64 --no-mysql
ambuild objdir

cd ../hl2sdk-mock
mkdir -p "$cwd/mock/gamedir"
bash build_gamedir.sh "$cwd/mock/gamedir" "$cwd/mock/metamod-source/objdir/package"
bash build_gamedir.sh "$cwd/mock/gamedir" "$cwd/mock/sourcemod/objdir/package"
mv $cwd/mock/gamedir/addons/sourcemod/plugins/*.smx $cwd/mock/gamedir/addons/sourcemod/plugins/disabled/
