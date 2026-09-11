#define _GNU_SOURCE
#include "cdrive.h"

// Spinner characters - rotating braille pattern
static const char *spinner_chars[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
static const int spinner_count = 10;

void *spinner_thread(void *arg) {
    LoadingSpinner *spinner = (LoadingSpinner *)arg;
    int i = 0;
    
    while (spinner->active) {
        printf("\r" COLOR_YELLOW "%s" COLOR_RESET " %s", spinner_chars[i], spinner->message);
        fflush(stdout);
        
        i = (i + 1) % spinner_count;
        cdrive_usleep(100000); // 100ms delay for smooth animation
    }
    
    // Clear the spinner line cleanly when done
    printf("\r\033[K");
    fflush(stdout);
    
    return NULL;
}

void start_spinner(LoadingSpinner *spinner, const char *message) {
    spinner->active = 0;
    strncpy(spinner->message, message, sizeof(spinner->message) - 1);
    spinner->message[sizeof(spinner->message) - 1] = '\0';
    
    spinner->active = 1;
    if (pthread_create(&spinner->thread, NULL, spinner_thread, spinner) != 0) {
        // If thread creation fails, reset active so stop_spinner doesn't join uninitialized thread
        spinner->active = 0;
        printf("%s\n", message);
    }
}

void stop_spinner(LoadingSpinner *spinner) {
    if (spinner->active) {
        spinner->active = 0;
        pthread_join(spinner->thread, NULL);
    }
}
