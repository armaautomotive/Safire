# Build Safire
if [[ "$OSTYPE" == "linux-gnu" ]]; then
    echo "Build Linux environment.";
    LD_LIBRARY_PATH=/usr/local/lib make linux $1 $2
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Build Mac environment."
    make $1 $2
elif [[ "$OSTYPE" == "win32" ]]; then
    echo "Build Windows environment."
    make $1 $2
elif [[ "$OSTYPE" == "freebsd"* ]]; then
    echo "Build FreeBSD"
    make $1 $2
else
    echo "Build..."
    make $1 $2
fi
