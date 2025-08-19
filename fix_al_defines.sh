#!/bin/bash

# Create a temporary file for the changes
cat > /tmp/al_defines.txt << 'END'
#define AL_NO_ERROR 0
#define AL_INITIAL 0x1011
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_FALSE 0
#define AL_TRUE 1
#define AL_FORMAT_MONO16 0x1101
#define AL_BUFFER 0x1009
#define AL_POSITION 0x1004
#define AL_GAIN 0x100A
#define AL_PITCH 0x1003
#define AL_LOOPING 0x1007
#define AL_REFERENCE_DISTANCE 0x1020
#define AL_ROLLOFF_FACTOR 0x1021
#define AL_MAX_DISTANCE 0x1023
#define AL_MIN_GAIN 0x100D
#define AL_MAX_GAIN 0x100E
END

# Replace the first occurrence
awk '
BEGIN { print_flag = 1 }
/#define AL_NO_ERROR 0/ {
  system("cat /tmp/al_defines.txt")
  print_flag = 0
}
/#define AL_BUFFER 0x1009/ { print_flag = 1; next }
{ if (print_flag) print }
' /workspaces/IKore-Engine/src/core/components/SoundComponent.h > /tmp/fixed_header.txt

# Then replace the second occurrence
awk '
BEGIN { 
  print_flag = 1 
  replaced = 0
}
/#define AL_NO_ERROR 0/ && replaced == 0 {
  print_flag = 1
  replaced = 1
  print
  next
}
/#define AL_NO_ERROR 0/ && replaced == 1 {
  system("cat /tmp/al_defines.txt")
  print_flag = 0
}
/#define AL_BUFFER 0x1009/ && replaced == 1 { print_flag = 1; next }
{ if (print_flag) print }
' /tmp/fixed_header.txt > /tmp/final_header.txt

# Apply the changes
mv /tmp/final_header.txt /workspaces/IKore-Engine/src/core/components/SoundComponent.h

echo "Fixes applied to SoundComponent.h"
