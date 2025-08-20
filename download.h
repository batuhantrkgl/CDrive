#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "cdrive.h"

// Function to download a file by its ID
int cdrive_pull_file_by_id(const char *file_id, const char *output_filename);

// Function to launch the interactive file browser and download a selected file
int cdrive_pull_interactive(void);

#endif // DOWNLOAD_H