/*
 * Animated GIFs Display Code for SmartMatrix and 32x32 RGB LED Panels
 *
 * This file contains code to enumerate and select animated GIF files by name
 *
 * Written by: Craig A. Lindley
 * 
 * Other references: https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/FSBrowser/FSBrowser.ino
 *
 */

#include "FilenameFunctions.h"

File file;

int numberOfFiles;

bool fileSeekCallback(unsigned long position) {
    return file.seek(position);
}

unsigned long filePositionCallback(void) {
    return file.position();
}

int fileReadCallback(void) {
    return file.read();
}

int fileReadBlockCallback(void * buffer, int numberOfBytes) {
    return file.read((uint8_t*)buffer, numberOfBytes);
}

bool isAnimationFile(const char filename []) {
    String filenameString(filename);

#if defined(ESP32)
    // ESP32 filename includes the full path, so need to remove the path before looking at the filename
    int pathindex = filenameString.lastIndexOf("/");
    if(pathindex >= 0)
        filenameString.remove(0, pathindex + 1);
#endif

    DBG_OUTPUT_PORT.print(filenameString);

    if ((filenameString[0] == '_') || (filenameString[0] == '~') || (filenameString[0] == '.')) {
        DBG_OUTPUT_PORT.println(" ignoring: leading _/~/. character");
        return false;
    }

    filenameString.toUpperCase();
    if (filenameString.endsWith(".GIF") != 1) {
        DBG_OUTPUT_PORT.println(" ignoring: doesn't end of .GIF");
        return false;
    }

    DBG_OUTPUT_PORT.println();

    return true;
}

// Enumerate and possibly display the animated GIF filenames in GIFS directory
int enumerateGIFFiles(const char *directoryName, boolean displayFilenames) {

    numberOfFiles = 0;

    File directory = FILESYSTEM.open(directoryName);
    if (!directory) {
        return -1;
    }

    File file = directory.openNextFile();
    while (file) {
        if (isAnimationFile(file.name())) {
            numberOfFiles++;
            if (displayFilenames) {
                DBG_OUTPUT_PORT.println(file.name());
            }
        }
        file.close();
        file = directory.openNextFile();
    }

    file.close();
    directory.close();

    return numberOfFiles;
}

// Get the full path/filename of the GIF file with specified index
void getGIFFilenameByIndex(const char *directoryName, int index, char *pnBuffer) {

    char* filename;

    // Make sure index is in range
    if ((index < 0) || (index >= numberOfFiles))
        return;

    File directory = FILESYSTEM.open(directoryName);
    if (!directory)
        return;

    File file = directory.openNextFile();
    while (file && (index >= 0)) {
        filename = (char*)file.name();

        if (isAnimationFile(file.name())) {
            index--;

#if !defined(ESP32)
            // Copy the directory name into the pathname buffer - ESP32 SD Library includes the full path name in the filename, so no need to add the directory name
            strcpy(pnBuffer, directoryName);
            // Append the filename to the pathname
            strcat(pnBuffer, filename);
#else
            strcpy(pnBuffer, filename);
#endif
        }

        file.close();
        file = directory.openNextFile();
    }

    file.close();
    directory.close();
}

int openGifFilenameByIndex(const char *directoryName, int index) {
    char pathname[30]; // long filename will break this... Smash the stack! i.e:
    /*
     * Stack smashing protect failure!
     * 
     * abort() was called at PC 0x400d9a90 on core 1
     * 
     * Backtrace: 0x40088578:0x3ffb1ec0 0x4008877b:0x3ffb1ee0 0x400d9a90:0x3ffb1f00 0x400d1d62:0x3ffb1f20 0x400d182f:0x3ffb1f80 0x400ef86e:0x3ffb1fa0
     * 
     * Rebooting...

     */

    getGIFFilenameByIndex(directoryName, index, pathname);
    
    DBG_OUTPUT_PORT.print("Pathname: ");
    DBG_OUTPUT_PORT.println(pathname);

    if(file)
    {
        file.close();
        DBG_OUTPUT_PORT.print("Closing old file...");
    }

    // Attempt to open the file for reading
        DBG_OUTPUT_PORT.print("Opening new file...");    
    file = FILESYSTEM.open(pathname);
    if (!file) {
        DBG_OUTPUT_PORT.println("Error opening GIF file");
        return -1;
    }

    return 0;
}


// Return a random animated gif path/filename from the specified directory
void chooseRandomGIFFilename(const char *directoryName, char *pnBuffer) {

    int index = random(numberOfFiles);
    getGIFFilenameByIndex(directoryName, index, pnBuffer);
}
