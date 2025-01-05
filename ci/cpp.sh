export TERM=${TERM:="xterm-256color"}

if [ -z ${CI_PROJECT_ROOT+x} ]; then
    SCRIPT_PATH="$(dirname $0)"
    CI_PROJECT_ROOT=$(realpath "$SCRIPT_PATH/..")
fi

if [ -z ${CI_BUILD_ROOT+x} ]; then
    CI_BUILD_ROOT=$(pwd)
fi

if [ -z ${CI_BUILD_TYPE+x} ]; then
    CI_BUILD_TYPE="Release"
fi


function log_success {
	echo -e "$(tput setaf 2)$1$(tput sgr0)"
}

function log_inform {
	echo -e "$(tput setaf 6)$1$(tput sgr0)"
}

function log_warning {
	echo -e "$(tput setaf 1)$1$(tput sgr0)"
}

function ci_clone {
    NAME=$1
    USER=$2
    REPO=$3
    BRANCH=$4
    log_inform "Cloning $NAME $USER/$REPO/$BRANCH"
    git clone https://github.com/$USER/$REPO "$CI_BUILD_ROOT/$REPO"
    cd "$CI_BUILD_ROOT/$REPO" || return 1
    git checkout $BRANCH
    git submodule update --init
    cd "$CI_BUILD_ROOT"
}

function ci_apt_install_build_deps {
    sudo apt update && sudo apt install ccache
}

function ci_prepare_all {
    mkdir -p $CI_BUILD_ROOT
    ci_clone "Pimoroni Pico" "pimoroni" "pimoroni-pico" "feature/picovector2-and-layers"
    ci_clone "Pico SDK" "raspberrypi" "pico-sdk" "master"
    ci_clone "Pico Extras" "raspberrypi" "pico-extras" "master"
    ci_clone "Presto" "pimoroni" "presto" "main"
}

function ci_debug {
    log_inform "Project root: $CI_PROJECT_ROOT"
    log_inform "Build root: $CI_BUILD_ROOT"
}

function ci_cmake_configure {
    PIMORONI_PICO_PATH="$CI_BUILD_ROOT/pimoroni-pico"
    PICO_SDK_PATH="$CI_BUILD_ROOT/pico-sdk"
    PIMORONI_PRESTO_PATH="$CI_BUILD_ROOT/presto"
    CMAKE_INSTALL_PREFIX="$CI_BUILD_ROOT/out"

    cmake $CI_PROJECT_ROOT -B "$CI_BUILD_ROOT" \
    -DPICOTOOL_FORCE_FETCH_FROM_GIT=1 \
    -DCMAKE_BUILD_TYPE=$CI_BUILD_TYPE \
    -DPIMORONI_PICO_PATH=$PIMORONI_PICO_PATH \
    -DPICO_SDK_PATH=$PICO_SDK_PATH \
    -DPIMORONI_PRESTO_PATH=$PIMORONI_PRESTO_PATH
}

function ci_cmake_build {
    cmake --build $CI_BUILD_ROOT -j 2
}

function ci_cmake_package {
    cmake --build $CI_BUILD_ROOT --target package -j 2
}