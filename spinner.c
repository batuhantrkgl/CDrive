#define _GNU_SOURCE
#include "cdrive.h"
#include <unistd.h>

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
        usleep(100000); // 100ms delay for smooth animation
    }
    
    // Clear the spinner line when done
    printf("\r");
    for (int j = 0; j < (int)strlen(spinner->message) + 10; j++) {
        printf(" ");
    }
    printf("\r");
    fflush(stdout);
    
    return NULL;
}

void start_spinner(LoadingSpinner *spinner, const char *message) {
    spinner->active = 1;
    strncpy(spinner->message, message, sizeof(spinner->message) - 1);
    spinner->message[sizeof(spinner->message) - 1] = '\0';
    
    if (pthread_create(&spinner->thread, NULL, spinner_thread, spinner) != 0) {
        // If thread creation fails, just print the message without animation
        printf("%s\n", message);
    }
}

void stop_spinner(LoadingSpinner *spinner) {
    if (spinner->active) {
        spinner->active = 0;
        pthread_join(spinner->thread, NULL);
    }
}
