#!/bin/bash
set -e
set -x

# Function to build for specified architecture
build_for_arch() {
    local arch=$1
    echo "Building for architecture: $arch"
    
    # Set up architecture-specific compiler and flags
    case $arch in
        "arm64")
            export CXX=aarch64-linux-gnu-g++
            export CFLAGS="-march=armv8-a -DDESKTOP_LINUX"
            export CXXFLAGS="-march=armv8-a -DDESKTOP_LINUX"
            export LIB_PATH="/usr/lib/aarch64-linux-gnu"
            ;;
        "armhf")
            export CXX=arm-linux-gnueabihf-g++
            export CFLAGS="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard"
            export CXXFLAGS="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard"
            export LIB_PATH="/usr/lib/arm-linux-gnueabihf"
            ;;
        "x86_64")
            export CXX=g++
            export CFLAGS="-march=x86-64 -DDESKTOP_LINUX"
            export CXXFLAGS="-march=x86-64 -DDESKTOP_LINUX"
            export LIB_PATH="/usr/lib/x86_64-linux-gnu"
            ;;
        "win64")
            export CXX=x86_64-w64-mingw32-g++-posix
            export CFLAGS="-m64 -march=x86-64"
            export CXXFLAGS="-m64 -march=x86-64 -static-libgcc -static-libstdc++ -pthread -D_WIN32 -DWIN32 -D_WINDOWS -D_GLIBCXX_HAS_GTHREADS -D_GLIBCXX_USE_C99_STDINT_TR1 -D_REENTRANT -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -L/opt/windows/allegro/allegro/lib -L/usr/x86_64-w64-mingw32/lib/libjpeg-turbo -L/usr/x86_64-w64-mingw32/lib"
            export LIB_PATH="/opt/windows/allegro/allegro/lib:/usr/x86_64-w64-mingw32/lib:/usr/x86_64-w64-mingw32/lib/libjpeg-turbo"
            export INCLUDE_PATH="/opt/windows/allegro/allegro/include"
            export ALLEGRO_LIBS="-lallegro_monolith-static -lwinmm -lgdi32 -lopengl32 -lole32 -ldsound -ldinput8 -ldxguid -lshlwapi -lpsapi -luuid -lpthread -lws2_32 -lmswsock -lpng16 -ljpeg -lwebp -lsharpyuv -lfreetype -lz"
            ;;
        *)
            echo "Unsupported Linux architecture: $arch"
            exit 1
            ;;
    esac

    # Create build directory
    mkdir -p /build/output/$arch
    cd /build/output/$arch

    # Source files
    CORE_FILES=(
        simvarDefs.cpp
        simvars.cpp
        globals.cpp
        knobs.cpp
        instrument.cpp
        instrument-panel.cpp
    )
    
    # Add full path to core files
    CORE_SOURCES=""
    for file in "${CORE_FILES[@]}"; do
        CORE_SOURCES+="/build/src/$file "
    done

    # Get instrument files
    INSTRUMENT_SOURCES=""
    for file in /build/src/instruments/*.cpp; do
        INSTRUMENT_SOURCES+="$file "
    done
    for subdir in /build/src/instruments/*/; do
        for file in "$subdir"*.cpp; do
            INSTRUMENT_SOURCES+="$file "
        done
    done

    # # Build the project
    # echo "Full build command:"
    # echo "$CXX $CXXFLAGS -v -o instrument-panel-$arch \
    #     -I /build/src -I /build/src/instruments \
    #     $CORE_SOURCES \
    #     $INSTRUMENT_SOURCES \
    #     -L$LIB_PATH \
    #     -lgpiod -lpthread -lallegro -lallegro_image -lallegro_font -lallegro_ttf"
    # should be taken care of by the -x flag at the start

    # Set up build flags based on target OS
    if [ "$arch" = "win64" ]; then
        $CXX $CXXFLAGS -o instrument-panel-$arch.exe \
            -I /build/src -I /build/src/instruments -I $INCLUDE_PATH \
            $CORE_SOURCES \
            $INSTRUMENT_SOURCES \
            $ALLEGRO_LIBS
    else
        # Common libraries for all Linux builds
        LINUX_LIBS="-lpthread -lallegro -lallegro_image -lallegro_font -lallegro_ttf"
        
        # Add gpiod only for ARM builds
        if [[ "$arch" == "arm"* ]]; then
            LINUX_LIBS="-lgpiod $LINUX_LIBS"
        fi

        $CXX $CXXFLAGS -o instrument-panel-$arch \
            -I /build/src -I /build/src/instruments \
            $CORE_SOURCES \
            $INSTRUMENT_SOURCES \
            -L$LIB_PATH \
            $LINUX_LIBS
    fi

    echo "Build command exit code: $?"

    echo "Build completed for $arch"
    cd /build
}

# Build for the specified architecture or all supported ones
if [ -z "$TARGET_ARCH" ] || [ "$TARGET_ARCH" = "all" ]; then
    build_for_arch "x86_64"
    build_for_arch "arm64"
    build_for_arch "armhf"
    build_for_arch "win64"
else
    build_for_arch "$TARGET_ARCH"
fi
