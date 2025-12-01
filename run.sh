
# 1. Clean previous build
echo "Cleaning old files..."
make clean

# 2. Compile new code
echo "Compiling project..."
make

# 3. Run if compile was successful
if [ $? -eq 0 ]; then
    echo "Build Successful. Launching Simulation..."
    ./main
else
    echo "Compilation Failed!"
    exit 1
fi