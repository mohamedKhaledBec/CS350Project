#!/bin/bash
# Install libraries

echo "Installing libraries..."
sudo apt-get update
sudo apt-get install -y libgtk-3-dev pkg-config
sudo apt install wkhtmltopdf

