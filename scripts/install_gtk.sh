#!/bin/bash
# Install GTK3 development libraries

echo "Installing GTK3 development libraries..."
sudo apt-get update
sudo apt-get install -y libgtk-3-dev pkg-config

echo ""
echo "GTK3 installation complete!"
echo "You can now build the popup GUI with: make popup"
