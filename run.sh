# Start Safire
if [[ "$OSTYPE" == "linux-gnu" ]]; then
    echo "Starting linux environment.";
    LD_LIBRARY_PATH=/usr/local/lib ./bin/Safire $1 $2
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Starting Mac environment."
    ./bin/Safire $1 $2
elif [[ "$OSTYPE" == "win32" ]]; then
    echo "Starting Windows environment."    
    ./bin/Safire $1 $2
elif [[ "$OSTYPE" == "freebsd"* ]]; then
    echo "FreeBSD"
    ./bin/Safire $1 $2        
else
    echo "Starting..."
    ./bin/Safire $1 $2     
fi


