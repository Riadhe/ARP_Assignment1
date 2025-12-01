#!/bin/bash

echo "Checking for Ncurses library..."

# Check for apt-get (Debian/Ubuntu)
if command -v apt-get >/dev/null; then
    echo "Detected apt-get. Installing libncurses-dev..."
    sudo apt-get update
    sudo apt-get install -y libncurses-dev libncurses5-dev
elif command -v yum >/dev/null; then
    # Fedora/CentOS
    echo "Detected yum. Installing ncurses-devel..."
    sudo yum install -y ncurses-devel
else
    echo "Could not detect package manager. Please install ncurses development libraries manually."
    exit 1
fi

echo "Installation complete."