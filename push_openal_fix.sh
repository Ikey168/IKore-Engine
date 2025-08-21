#!/bin/bash
# Script to push the OpenAL fixes to the remote repository

# Check if we're in the right directory
if [ ! -d ".git" ]; then
  echo "Error: Not in a git repository root. Please run from the IKore-Engine directory."
  exit 1
fi

# Push to the remote repository
git push origin 82-sound-component

echo "Changes pushed to remote repository."
echo "Check the CI status at: https://github.com/Ikey168/IKore-Engine/pull/130"
