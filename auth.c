#define _GNU_SOURCE
#include <errno.h>
#include "cdrive.h"

// Platform-specific function definitions, moved from cdrive.h to be local to this file.
#ifdef _WIN32 // Windows specific definitions
    // Initialize Winsock
    static int init_winsock(void) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
            fprintf(stderr, "WSAStartup failed. Error Code : %d\n", WSAGetLastError());
            return 1;
        }
        return 0;
    }
    static void cleanup_winsock(void) {
        WSACleanup();
    }

    // Windows console handle
    static HANDLE hConsole = INVALID_HANDLE_VALUE;
    static DWORD dwOriginalMode = 0;
    
    static void disable_raw_mode(void);

    static void init_console(void) {
        if (hConsole == INVALID_HANDLE_VALUE) {
            hConsole = GetStdHandle(STD_INPUT_HANDLE); // Use STD_INPUT_HANDLE for console mode functions
            GetConsoleMode(hConsole, &dwOriginalMode);
            atexit(disable_raw_mode);
        }
    }
    
    static void enable_raw_mode(void) {
        init_console();
        DWORD dwMode = dwOriginalMode;
        dwMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT); // Disable echo and line buffering
        SetConsoleMode(hConsole, dwMode);
    }
    
    static void disable_raw_mode(void) {
        if (hConsole != INVALID_HANDLE_VALUE) {
            SetConsoleMode(hConsole, dwOriginalMode);
        }
    }
    
    static int platform_getchar(void) {
        return _getch();
    }

    static int cdrive_getch_timeout(int timeout_ms) {
        int waited = 0;
        while (!_kbhit()) {
            if (waited >= timeout_ms) return -1;
            Sleep(5);
            waited += 5;
        }
        return _getch();
    }

#else // For Linux/macOS (non-Windows)
    // Dummy Winsock functions for non-Windows
    static int init_winsock(void) { return 0; }
    static void cleanup_winsock(void) {}

    // Raw mode functions for Unix-like systems
    static struct termios original_termios;
    static int raw_mode_enabled = 0;
    static int raw_mode_initialized = 0;

    static void disable_raw_mode(void) {
        if (raw_mode_enabled) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
            raw_mode_enabled = 0;
        }
    }

    static void sig_cleanup_handler(int sig) {
        disable_raw_mode();
        signal(sig, SIG_DFL);
        raise(sig);
    }

    static void enable_raw_mode(void) {
        if (!raw_mode_initialized) {
            tcgetattr(STDIN_FILENO, &original_termios);
            atexit(disable_raw_mode);
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = sig_cleanup_handler;
            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGTERM, &sa, NULL);
            sigaction(SIGHUP, &sa, NULL);
            sigaction(SIGQUIT, &sa, NULL);
            raw_mode_initialized = 1;
        }
        if (!raw_mode_enabled) {
            struct termios raw = original_termios;
            raw.c_lflag &= ~(ECHO | ICANON);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            raw_mode_enabled = 1;
        }
    }

    static int cdrive_getch_timeout(int timeout_ms) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            unsigned char c;
            if (read(STDIN_FILENO, &c, 1) == 1) return c;
        }
        return -1;
    }
#endif // End of platform-specific block

static void menu_move_up(int *selected, int *start_index, int num_options, int display_window_size) {
    if (*selected > 0) {
        (*selected)--;
        if (*selected < *start_index) {
            *start_index = *selected;
        }
    } else {
        *selected = num_options - 1;
        if (num_options > display_window_size) {
            *start_index = num_options - display_window_size;
        } else {
            *start_index = 0;
        }
    }
}

static void menu_move_down(int *selected, int *start_index, int num_options, int display_window_size) {
    if (*selected < num_options - 1) {
        (*selected)++;
        if (*selected >= *start_index + display_window_size) {
            *start_index = *selected - display_window_size + 1;
        }
    } else {
        *selected = 0;
        *start_index = 0;
    }
}

int show_interactive_menu(const char *question, const char **options, int num_options) {
    int selected = 0;
    int start_index = 0;
    int display_window_size = 10;

    if (num_options < display_window_size) {
        display_window_size = num_options;
    }

    enable_raw_mode();

    while (1) {
        printf("\033[2J\033[1;1H");
        print_colored("[?] ", COLOR_CYAN);
        print_colored(question, COLOR_BOLD);
        printf("\n\n");
        printf("  (Use %s↑/↓%s arrows to move, %sEnter%s to select, %s'q'%s to quit)\n\n",
               COLOR_YELLOW, COLOR_RESET, COLOR_GREEN, COLOR_RESET, COLOR_RED, COLOR_RESET);

        for (int i = start_index; i < start_index + display_window_size && i < num_options; i++) {
            if (i == selected) {
                print_colored("  > ", COLOR_GREEN);
                print_colored(options[i], COLOR_BOLD);
                printf("\n");
            } else {
                printf("    %s\n", options[i]);
            }
        }

        printf("\n");

        if (start_index > 0) {
            printf("  %s... (more above)%s\n", COLOR_YELLOW, COLOR_RESET);
        }
        if (start_index + display_window_size < num_options) {
            printf("  %s... (more below)%s\n", COLOR_YELLOW, COLOR_RESET);
        }

        fflush(stdout);

        int ch = cdrive_getch();
        if (ch < 0) {
            continue;
        }

        if (ch == '\033') {
            int s0 = cdrive_getch_timeout(50);
            if (s0 < 0) {
                disable_raw_mode();
                printf("\033[2J\033[1;1H");
                print_colored("[!] ", COLOR_YELLOW);
                printf("Selection cancelled.\n");
                return -1;
            }
            int s1 = cdrive_getch_timeout(50);
            if (s0 == '[' && s1 >= 0) {
                if (s1 == 'A') {
                    menu_move_up(&selected, &start_index, num_options, display_window_size);
                } else if (s1 == 'B') {
                    menu_move_down(&selected, &start_index, num_options, display_window_size);
                }
            }
#ifdef _WIN32
        } else if (ch == 0xE0 || ch == 0x00) {
            int seq = cdrive_getch();
            if (seq == 0x48) {
                menu_move_up(&selected, &start_index, num_options, display_window_size);
            } else if (seq == 0x50) {
                menu_move_down(&selected, &start_index, num_options, display_window_size);
            }
#endif
        } else {
            switch (ch) {
                case 'k':
                case 'K':
                    menu_move_up(&selected, &start_index, num_options, display_window_size);
                    break;
                case 'j':
                case 'J':
                    menu_move_down(&selected, &start_index, num_options, display_window_size);
                    break;
                case '\n':
                case '\r':
                    disable_raw_mode();
                    printf("\033[2J\033[1;1H");
                    print_colored("[>] ", COLOR_GREEN);
                    print_colored("Selected: ", COLOR_BOLD);
                    printf("%s\n\n", options[selected]);
                    return selected;
                case 'q':
                case 'Q':
                    disable_raw_mode();
                    printf("\033[2J\033[1;1H");
                    print_colored("[!] ", COLOR_YELLOW);
                    printf("Selection cancelled.\n");
                    return -1;
            }
        }
    }
}


// Color printing functions
void print_colored(const char *text, const char *color) {
    printf("%s%s%s", color, text, COLOR_RESET);
}

void print_success(const char *message) {
    print_colored("[+] ", COLOR_GREEN);
    printf("%s\n", message);
}

void print_error(const char *message) {
    print_colored("[!] ", COLOR_RED);
    printf("%s\n", message);
}

void print_warning(const char *message) {
    print_colored("[!] ", COLOR_YELLOW);
    printf("%s\n", message);
}

void print_info(const char *message) {
    print_colored("[i] ", COLOR_BLUE);
    printf("%s\n", message);
}

void print_header(const char *title) {
    printf("\n%s%s %s %s\n", COLOR_BG_BLUE, COLOR_WHITE, title, COLOR_RESET);
    for (int i = 0; i < (int)strlen(title) + 2; i++) printf("%s-%s", COLOR_BLUE, COLOR_RESET);
    printf("\n\n");
}

int setup_config_dir(void) {
    char config_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    
    if (!home_dir) {
        print_error("Unable to determine home directory");
        return -1;
    }
    
    snprintf(config_path, sizeof(config_path), "%s%s%s", home_dir, PATH_SEP, CONFIG_DIR);
    
    struct stat st = {0};
    if (stat(config_path, &st) == -1) {
        if (mkdir(config_path, 0700) != 0) {
            perror("Error creating config directory");
            return -1;
        }
    }
    
    return 0;
}

static int interactive_credential_setup(void) {
    const char *auth_options[] = {
        "I have OAuth2 credentials (client_id and client_secret)",
        "I need help setting up OAuth2 credentials",
        "Exit"
    };
    
    int choice = show_interactive_menu("? How would you like to authenticate Google Drive?", auth_options, 3);
    
    if (choice == -1 || choice == 2) {
        printf("Authentication cancelled.\n");
        return -1;
    }
    
    if (choice == 1) {
        // Show help
        printf("\n");
        print_info("Setting up Google Drive OAuth2 credentials:");
        printf("\n");
        printf("1. Go to: %shttps://console.cloud.google.com/%s\n", COLOR_BLUE, COLOR_RESET);
        printf("%s2. Create a new project or select an existing one%s\n", COLOR_BOLD, COLOR_RESET);
        printf("%s3. Enable the Google Drive API:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("   - Navigate to %sAPIs & Services > Library%s\n", COLOR_BLUE, COLOR_RESET);
        printf("   - Search for %s'Google Drive API'%s and enable it\n", COLOR_BLUE, COLOR_RESET);
        printf("%s4. Create OAuth2 credentials:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("   - Go to APIs & Services > Credentials\n");
        printf("   - Click %s'Create Credentials' > 'OAuth 2.0 Client IDs'%s\n", COLOR_BLUE, COLOR_RESET);
        printf("   - Choose %s'Desktop application'%s\n", COLOR_BLUE, COLOR_RESET);
        printf("   - Add redirect URI: %shttp://localhost:8080%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("%s5. Download the credentials JSON file%s\n\n", COLOR_BOLD, COLOR_RESET);
        printf("Tip: Look for 'client_id' and 'client_secret' in the downloaded JSON\n\n");
        printf("After setup, run %s'cdrive auth login'%s again.\n", COLOR_YELLOW, COLOR_RESET);
        return -1;
    }
    
    // Get credentials interactively
    char client_id[512];
    char client_secret[256];
    char config_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    
    snprintf(config_path, sizeof(config_path), "%s%s%s%s%s", home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, CLIENT_ID_FILE);
    
    printf("\n");
    
    // Ask for Client ID
    printf("%s? Client ID:%s ", COLOR_CYAN, COLOR_RESET);
    if (!fgets(client_id, sizeof(client_id), stdin)) {
        print_error("Failed to read client ID");
        return -1;
    }
    client_id[strcspn(client_id, "\n")] = 0; // Remove newline
    
    // Ask for Client Secret with echo disabled
    printf("%s? Client Secret (hidden):%s ", COLOR_CYAN, COLOR_RESET);
    fflush(stdout);
#ifdef _WIN32
    size_t sec_idx = 0;
    int sec_ch;
    while ((sec_ch = _getch()) != '\r' && sec_ch != '\n' && sec_ch != EOF) {
        if (sec_ch == '\b' && sec_idx > 0) {
            sec_idx--;
        } else if (sec_idx < sizeof(client_secret) - 1 && sec_ch >= 32) {
            client_secret[sec_idx++] = (char)sec_ch;
        }
    }
    client_secret[sec_idx] = '\0';
    printf("\n");
#else
    struct termios old_t, no_echo_t;
    tcgetattr(STDIN_FILENO, &old_t);
    no_echo_t = old_t;
    no_echo_t.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &no_echo_t);
    if (!fgets(client_secret, sizeof(client_secret), stdin)) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
        print_error("Failed to read client secret");
        return -1;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
    printf("\n");
    client_secret[strcspn(client_secret, "\r\n")] = '\0';
#endif
    
    // Validate input
    if (strlen(client_id) < 10 || strlen(client_secret) < 10) {
        print_error("Invalid credentials. Please check your input.");
        return -1;
    }
    
    // Save to file with secure permissions (0600)
#ifdef _WIN32
    FILE *file = fopen(config_path, "w");
#else
    int fd = open(config_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    FILE *file = (fd >= 0) ? fdopen(fd, "w") : NULL;
#endif
    if (!file) {
        print_error("Error creating credentials file");
        return -1;
    }
    
    json_object *creds_obj = json_object_new_object();
    json_object_object_add(creds_obj, "client_id", json_object_new_string(client_id));
    json_object_object_add(creds_obj, "client_secret", json_object_new_string(client_secret));
    fprintf(file, "%s\n", json_object_to_json_string_ext(creds_obj, JSON_C_TO_STRING_PRETTY));
    json_object_put(creds_obj);
    if (fclose(file) != 0) {
        print_error("Failed to write client credentials to disk");
        return -1;
    }
    
    print_colored("[+] ", COLOR_GREEN);
    printf("Credentials saved successfully!\n");
    return 0;
}

int load_client_credentials(ClientCredentials *creds) {
    char config_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    if (!home_dir) {
        print_error("Unable to determine home directory");
        return -1;
    }
    
    snprintf(config_path, sizeof(config_path), "%s%s%s%s%s", home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, CLIENT_ID_FILE);
    
    FILE *file = fopen(config_path, "r");
    if (!file) {
        // Try interactive setup first
        if (interactive_credential_setup() != 0) {
            return -1;
        }
        // Now try to open the file again after interactive setup
        file = fopen(config_path, "r");
        if (!file) {
            print_error("Failed to load credentials after setup");
            return -1;
        }
    }
    
    char buffer[2048];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[bytes_read] = '\0';
    
    json_object *root = json_tokener_parse(buffer);
    if (!root) {
        print_error("Error parsing client credentials file");
        return -1;
    }
    
    json_object *client_id_obj = NULL, *client_secret_obj = NULL;
    json_object *installed_obj = NULL, *web_obj = NULL;
    json_object *container = root;

    if (json_object_object_get_ex(root, "installed", &installed_obj)) {
        container = installed_obj;
    } else if (json_object_object_get_ex(root, "web", &web_obj)) {
        container = web_obj;
    }

    if (!json_object_object_get_ex(container, "client_id", &client_id_obj) ||
        !json_object_object_get_ex(container, "client_secret", &client_secret_obj)) {
        print_error("Invalid client credentials format");
        json_object_put(root);
        return -1;
    }
    
    strncpy(creds->client_id, json_object_get_string(client_id_obj), sizeof(creds->client_id) - 1);
    strncpy(creds->client_secret, json_object_get_string(client_secret_obj), sizeof(creds->client_secret) - 1);
    
    // Ensure null termination
    creds->client_id[sizeof(creds->client_id) - 1] = '\0';
    creds->client_secret[sizeof(creds->client_secret) - 1] = '\0';
    
    json_object_put(root);
    return 0;
}

size_t write_response_callback(char *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    APIResponse *response = (APIResponse *)userp;

    char *new_data = realloc(response->data, response->size + total_size + 1);
    
    if (!new_data) {
        print_error("Failed to allocate memory for response");
        return 0;
    }
    
    response->data = new_data;
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = '\0';
    
    return total_size;
}

int cdrive_api_get(const char *url, APIResponse *response) {
    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) break;
        }

        CURL *curl = curl_easy_init();
        if (!curl) {
            if (response->data) {
                free(response->data);
                response->data = NULL;
                response->size = 0;
            }
            return -1;
        }

        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
        struct curl_slist *headers = curl_slist_append(NULL, auth_header);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && http_code == 200) return 0;
        if (res != CURLE_OK || (http_code != 401 && http_code != 403)) break;

        // Auth error — clean response data for retry
        if (response->data) { free(response->data); response->data = NULL; response->size = 0; }
    }

    if (response->data) { free(response->data); response->data = NULL; response->size = 0; }
    return -1;
}

static int open_browser_url(const char *url) {
#ifdef _WIN32
    HINSTANCE res = ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    return ((intptr_t)res > 32) ? 0 : -1;
#else
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
#ifdef __APPLE__
        char *const args_open[] = {"open", (char *)url, NULL};
        execvp("open", args_open);
#else
        char *const args_xdg[] = {"xdg-open", (char *)url, NULL};
        execvp("xdg-open", args_xdg);
        char *const args_open[] = {"open", (char *)url, NULL};
        execvp("open", args_open);
#endif
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
    }
    return -1;
#endif
}

int start_local_server(char *auth_code, size_t max_code_len, const char *auth_url, int open_browser) {
    cdrive_socket_t server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[4096] = {0};
    
    const char *response_html =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html><html><head><title>cdrive Authentication</title>"
        "<style>body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;background:#f5f5f5;}"
        "h1{color:#4285f4;font-size:2em;}p{color:#666;font-size:1.1em;margin:20px;}"
        ".success{background:#d4edda;border:1px solid #c3e6cb;border-radius:5px;padding:20px;margin:20px auto;max-width:500px;}"
        "</style></head><body>"
        "<div class='success'><h1>✅ Authentication Successful!</h1>"
        "<p>You can now close this window and return to your terminal.</p>"
        "<p>The cdrive CLI tool is now authenticated and ready to use.</p></div>"
        "</body></html>";
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == CDRIVE_INVALID_SOCKET) {
        perror("socket failed");
        return -1;
    }
    
    // Set socket options
#ifdef _WIN32
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt))) {
#else
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
#endif
        perror("setsockopt");
        cdrive_socket_close(server_fd);
        return -1;
    }

#ifdef _WIN32
    DWORD rcv_timeout = 120000;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rcv_timeout, sizeof(rcv_timeout));
#else
    struct timeval tv = { .tv_sec = 120, .tv_usec = 0 };
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(8080);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEADDRINUSE) {
            print_error("Port 8080 is already in use by another application.");
            print_info("Please free port 8080 or use 'cdrive auth login --no-browser'.");
        } else {
            perror("bind failed");
        }
#else
        if (errno == EADDRINUSE) {
            print_error("Port 8080 is already in use by another application.");
            print_info("Please free port 8080 or use 'cdrive auth login --no-browser'.");
        } else {
            perror("bind failed");
        }
#endif
        cdrive_socket_close(server_fd);
        return -1;
    }
    
    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        cdrive_socket_close(server_fd);
        return -1;
    }
    
    LoadingSpinner spinner = {0};
    if (open_browser) {
        print_colored("[*] ", COLOR_YELLOW);
        printf("Starting authentication server...\n");
        start_spinner(&spinner, "Waiting for authentication callback...");
    }
    
    // Open browser if requested
    if (open_browser) {
        // Temporarily stop spinner for clean browser opening message
        stop_spinner(&spinner);
        
        printf("\n");  // Ensure we're on a new line
        print_colored("\n[>] ", COLOR_GREEN);
        printf("Opening browser...\n");
        int browser_result = open_browser_url(auth_url);
        
        if (browser_result != 0) {
            print_warning("Could not automatically open browser. Please copy the URL above and paste it into your browser manually.");
        }
        
        // Restart spinner after browser message
        start_spinner(&spinner, "Waiting for authentication callback...");
    }
    
    // Accept connection loop
    while (strlen(auth_code) == 0) {
        addrlen = sizeof(address);
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) == CDRIVE_INVALID_SOCKET) {
#ifdef _WIN32
            int wsa_err = WSAGetLastError();
            if (wsa_err == WSAEINTR) continue;
            if (wsa_err == WSAEWOULDBLOCK || wsa_err == WSAETIMEDOUT) {
                if (open_browser) stop_spinner(&spinner);
                print_error("Authentication timed out after 2 minutes.");
                break;
            }
#else
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (open_browser) stop_spinner(&spinner);
                print_error("Authentication timed out after 2 minutes.");
                break;
            }
#endif
            perror("accept");
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        int bytes_read = cdrive_socket_read(new_socket, buffer, (int)sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            cdrive_socket_close(new_socket);
            continue;
        }
        buffer[bytes_read] = '\0';

        // Check if user denied or cancelled authorization
        char *error_start = strstr(buffer, "error=");
        if (error_start) {
            error_start += 6;
            char *error_end = strchr(error_start, '&');
            if (!error_end) error_end = strchr(error_start, ' ');
            size_t err_len = error_end ? (size_t)(error_end - error_start) : strlen(error_start);
            char err_buf[128];
            if (err_len >= sizeof(err_buf)) err_len = sizeof(err_buf) - 1;
            memcpy(err_buf, error_start, err_len);
            err_buf[err_len] = '\0';

            const char *error_html =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                "<!DOCTYPE html><html><body><h1>Authentication Cancelled</h1>"
                "<p>Authorization was denied or cancelled.</p></body></html>";
            cdrive_socket_write(new_socket, error_html, (int)strlen(error_html));
            cdrive_socket_close(new_socket);

            if (open_browser) stop_spinner(&spinner);
            print_error("Authorization was denied or cancelled");
            fprintf(stderr, "Reason: %s\n", err_buf);
            break;
        }

        // Parse authorization code from request
        char *code_start = strstr(buffer, "code=");
        if (code_start) {
            code_start += 5; // Skip "code="
            char *code_end = strchr(code_start, '&');
            if (!code_end) code_end = strchr(code_start, ' ');
            size_t code_len = code_end ? (size_t)(code_end - code_start) : strlen(code_start);
            char raw_code[1024];
            if (code_len >= sizeof(raw_code)) code_len = sizeof(raw_code) - 1;
            memcpy(raw_code, code_start, code_len);
            raw_code[code_len] = '\0';

            char *decoded_code = url_decode(raw_code);
            const char *final_code = decoded_code ? decoded_code : raw_code;
            strncpy(auth_code, final_code, max_code_len - 1);
            auth_code[max_code_len - 1] = '\0';
            if (decoded_code) free(decoded_code);

            // Send success response
            cdrive_socket_write(new_socket, response_html, (int)strlen(response_html));
            cdrive_socket_close(new_socket);
            break;
        } else {
            // Not the OAuth callback (e.g. browser probe, favicon) - return 204 and keep listening
            const char *noop_resp = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
            cdrive_socket_write(new_socket, noop_resp, (int)strlen(noop_resp));
            cdrive_socket_close(new_socket);
        }
    }

    if (open_browser) {
        stop_spinner(&spinner);
    }
    cdrive_socket_close(server_fd);

    return strlen(auth_code) > 0 ? 0 : -1;
}

char *url_encode(const char *str) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    
    char *encoded = curl_easy_escape(curl, str, 0);
    char *result = encoded ? strdup(encoded) : NULL;
    
    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);
    
    return result;
}

char *url_decode(const char *str) {
    if (!str) return NULL;
    CURL *curl = curl_easy_init();
    if (!curl) return strdup(str);
    int outlen = 0;
    char *decoded = curl_easy_unescape(curl, str, 0, &outlen);
    char *result = decoded ? strdup(decoded) : strdup(str);
    if (decoded) curl_free(decoded);
    curl_easy_cleanup(curl);
    return result;
}

static int exchange_code_for_tokens(const char *auth_code, OAuthTokens *tokens) {
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        print_error("Error initializing curl");
        return -1;
    }
    
    // URL encode all parameters
    char *encoded_code = url_encode(auth_code);
    char *encoded_client_id = url_encode(g_client_creds.client_id);
    char *encoded_client_secret = url_encode(g_client_creds.client_secret);
    char *encoded_redirect = url_encode(REDIRECT_URI);
    
    if (!encoded_code || !encoded_client_id || !encoded_client_secret || !encoded_redirect) {
        print_error("Error encoding parameters");
        if (encoded_code) free(encoded_code);
        if (encoded_client_id) free(encoded_client_id);
        if (encoded_client_secret) free(encoded_client_secret);
        if (encoded_redirect) free(encoded_redirect);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Prepare POST data
    char post_data[2048];
    snprintf(post_data, sizeof(post_data),
        "code=%s&client_id=%s&client_secret=%s&redirect_uri=%s&grant_type=authorization_code",
        encoded_code, encoded_client_id, encoded_client_secret, encoded_redirect);
    
    free(encoded_code);
    free(encoded_client_id);
    free(encoded_client_secret);
    free(encoded_redirect);
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, OAUTH_TOKEN_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    
    print_colored("[>] ", COLOR_BLUE);
    printf("Exchanging authorization code for access tokens...\n");
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Check HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        print_error("Error exchanging code");
        printf("Details: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
        return -1;
    }
    
    // Check HTTP status (debug output removed)
    
    if (http_code != 200) {
        print_error("HTTP error during token exchange");
        if (response.data) {
            json_object *err_root = json_tokener_parse(response.data);
            if (err_root) {
                json_object *err_obj = NULL, *desc_obj = NULL;
                if (json_object_object_get_ex(err_root, "error", &err_obj)) {
                    fprintf(stderr, "Error: %s\n", json_object_get_string(err_obj));
                }
                if (json_object_object_get_ex(err_root, "error_description", &desc_obj)) {
                    fprintf(stderr, "Description: %s\n", json_object_get_string(desc_obj));
                }
                json_object_put(err_root);
            }
            free(response.data);
        }
        return -1;
    }
    
    // Parse JSON response
    json_object *root = json_tokener_parse(response.data);
    if (!root) {
        print_error("Error parsing token response");
        if (response.data) free(response.data);
        return -1;
    }
    
    json_object *access_token_obj, *refresh_token_obj, *token_type_obj, *expires_in_obj;
    
    if (json_object_object_get_ex(root, "access_token", &access_token_obj)) {
        strncpy(tokens->access_token, json_object_get_string(access_token_obj), 
                sizeof(tokens->access_token) - 1);
        tokens->access_token[sizeof(tokens->access_token) - 1] = '\0';
    }
    
    if (json_object_object_get_ex(root, "refresh_token", &refresh_token_obj)) {
        strncpy(tokens->refresh_token, json_object_get_string(refresh_token_obj), 
                sizeof(tokens->refresh_token) - 1);
        tokens->refresh_token[sizeof(tokens->refresh_token) - 1] = '\0';
    }
    
    if (json_object_object_get_ex(root, "token_type", &token_type_obj)) {
        strncpy(tokens->token_type, json_object_get_string(token_type_obj), 
                sizeof(tokens->token_type) - 1);
        tokens->token_type[sizeof(tokens->token_type) - 1] = '\0';
    }
    
    if (json_object_object_get_ex(root, "expires_in", &expires_in_obj)) {
        tokens->expires_in = json_object_get_int(expires_in_obj);
    }
    
    json_object_put(root);
    if (response.data) free(response.data);
    
    return strlen(tokens->access_token) > 0 ? 0 : -1;
}

int refresh_access_token(OAuthTokens *tokens) {
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};

    if (strlen(tokens->refresh_token) == 0) {
        // No refresh token, can't proceed.
        return -1;
    }

    // Load client credentials if not already loaded
    if (strlen(g_client_creds.client_id) == 0) {
        if (load_client_credentials(&g_client_creds) != 0) {
            return -1;
        }
    }

    curl = curl_easy_init();
    if (!curl) {
        print_error("Error initializing curl for token refresh");
        return -1;
    }

    char *encoded_client_id = url_encode(g_client_creds.client_id);
    char *encoded_client_secret = url_encode(g_client_creds.client_secret);
    char *encoded_refresh_token = url_encode(tokens->refresh_token);

    if (!encoded_client_id || !encoded_client_secret || !encoded_refresh_token) {
        print_error("Error encoding parameters for token refresh");
        if (encoded_client_id) free(encoded_client_id);
        if (encoded_client_secret) free(encoded_client_secret);
        if (encoded_refresh_token) free(encoded_refresh_token);
        curl_easy_cleanup(curl);
        return -1;
    }

    // Prepare POST data
    char post_data[2048];
    snprintf(post_data, sizeof(post_data),
        "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
        encoded_client_id, encoded_client_secret, encoded_refresh_token);

    free(encoded_client_id);
    free(encoded_client_secret);
    free(encoded_refresh_token);

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, OAUTH_TOKEN_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // Perform request
    res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        if (response.data) free(response.data);
        return -1;
    }

    // Parse JSON response
    json_object *root = json_tokener_parse(response.data);
    if (!root) {
        if (response.data) free(response.data);
        return -1;
    }

    json_object *access_token_obj, *expires_in_obj;
    if (json_object_object_get_ex(root, "access_token", &access_token_obj)) {
        strncpy(tokens->access_token, json_object_get_string(access_token_obj), sizeof(tokens->access_token) - 1);
        tokens->access_token[sizeof(tokens->access_token) - 1] = '\0';
    }

    if (json_object_object_get_ex(root, "expires_in", &expires_in_obj)) {
        tokens->expires_in = json_object_get_int(expires_in_obj);
    }

    json_object_put(root);
    if (response.data) free(response.data);

    return 0;
}

int cdrive_auth_login(int headless) {
    char auth_url[MAX_URL_SIZE];
    char auth_code[256] = {0};
    OAuthTokens tokens = {0};

    printf("\n");

    // Load client credentials (will prompt interactively if not found)
    if (load_client_credentials(&g_client_creds) != 0) {
        return -1;
    }

    // Check if credentials were loaded properly
    if (strlen(g_client_creds.client_id) < 10) {
        print_error("Invalid or missing client_id. Please check your credentials.");
        printf("Current client_id length: %zu\n", strlen(g_client_creds.client_id));
        return -1;
    }

    if (strlen(g_client_creds.client_secret) < 10) {
        print_error("Invalid or missing client_secret. Please check your credentials.");
        return -1;
    }

    print_colored("[+] ", COLOR_GREEN);
    printf("Client credentials loaded successfully\n");

    // Check if running in an SSH session and provide guidance
    if (!headless && (getenv("SSH_CLIENT") || getenv("SSH_CONNECTION"))) {
        printf("\n");
        print_warning("It looks like you're running in an SSH session.");
        printf("\n");
        print_info("For browser authentication to work, you must forward port 8080 from your");
        print_info("local machine to this server. You can do this when you connect via SSH.");
        printf("\n");
        print_colored("  $ ", COLOR_CYAN);
        printf("ssh -L 8080:localhost:8080 user@your_server_ip\n\n");
        print_info("If you have already done this, you can proceed.");
        print_info("If not, please exit (Ctrl+C), reconnect with the command above,");
        print_info("and then run 'cdrive auth login' again.");
        printf("\n");
    }

    print_colored("[*] ", COLOR_BLUE);
    printf("Starting Google Drive authentication...\n\n");

    // URL encode parameters
    char *encoded_client_id = url_encode(g_client_creds.client_id);
    char *encoded_redirect = url_encode(REDIRECT_URI);
    char *encoded_scope = url_encode(SCOPE);

    if (!encoded_client_id || !encoded_redirect || !encoded_scope) {
        if (encoded_client_id) free(encoded_client_id);
        if (encoded_redirect) free(encoded_redirect);
        if (encoded_scope) free(encoded_scope);
        print_error("Error encoding parameters");
        return -1;
    }

    // Build authorization URL
    snprintf(auth_url, sizeof(auth_url),
        "%s?client_id=%s&redirect_uri=%s&scope=%s&response_type=code&access_type=offline&prompt=consent",
        OAUTH_AUTH_URL, encoded_client_id, encoded_redirect, encoded_scope);

    free(encoded_client_id);
    free(encoded_redirect);
    free(encoded_scope);

    if (headless) {
        print_warning("Running in headless mode. Please follow the instructions below.");
        printf("\n");
        print_info("1. Open the following URL in your browser:");
        printf("%s\n\n", auth_url);
        print_info("2. After authenticating, you will be redirected to a URL that looks like 'http://localhost:8080/?code=...'.");
        print_info("3. Copy the entire redirected URL from your browser's address bar and paste it below.");
        printf("\n");

        char redirected_url[MAX_URL_SIZE];
        printf("%s? Enter the redirected URL:%s ", COLOR_CYAN, COLOR_RESET);
        if (!fgets(redirected_url, sizeof(redirected_url), stdin)) {
            print_error("Failed to read the redirected URL.");
            return -1;
        }
        redirected_url[strcspn(redirected_url, "\n")] = 0; // Remove newline

        char *code_start = strstr(redirected_url, "code=");
        if (code_start) {
            code_start += 5; // Skip "code="
            char *code_end = strchr(code_start, '&');
            if (!code_end) {
                code_end = strchr(code_start, ' ');
            }
            size_t code_len = code_end ? (size_t)(code_end - code_start) : strlen(code_start);
            char raw_code[1024];
            if (code_len >= sizeof(raw_code)) code_len = sizeof(raw_code) - 1;
            memcpy(raw_code, code_start, code_len);
            raw_code[code_len] = '\0';

            char *decoded_code = url_decode(raw_code);
            const char *final_code = decoded_code ? decoded_code : raw_code;
            strncpy(auth_code, final_code, sizeof(auth_code) - 1);
            auth_code[sizeof(auth_code) - 1] = '\0';
            if (decoded_code) free(decoded_code);
        } else if (strlen(redirected_url) > 0) {
            // User pasted the raw authorization code directly
            const char *raw = redirected_url;
            while (*raw == ' ' || *raw == '\t') raw++;
            char *decoded_code = url_decode(raw);
            const char *final_code = decoded_code ? decoded_code : raw;
            strncpy(auth_code, final_code, sizeof(auth_code) - 1);
            auth_code[sizeof(auth_code) - 1] = '\0';
            if (decoded_code) free(decoded_code);
            size_t len = strlen(auth_code);
            while (len > 0 && (auth_code[len - 1] == ' ' || auth_code[len - 1] == '\r' || auth_code[len - 1] == '\t')) {
                auth_code[--len] = '\0';
            }
        }

    } else {
        print_warning("First, authenticate in your web browser");
        printf("Press ");
        print_colored("Enter", COLOR_BOLD);
        printf(" to open Google's authorization page in your browser...\n\n");

        // Wait for user to press Enter
        getchar();

        // Start local server to receive callback FIRST
        print_colored("[*] ", COLOR_BLUE);
        printf("Starting local server on port 8080...\n");

        // Initialize socket subsystem
        if (init_winsock() != 0) {
            print_error("Failed to initialize network subsystem");
            return -1;
        }

#ifdef _WIN32
        // Windows: Use simple synchronous approach (no fork)
        // Start server and wait for callback (spinner is handled inside start_local_server)
        if (start_local_server(auth_code, sizeof(auth_code), auth_url, 1) != 0) {
            print_error("Failed to receive authorization callback");
            cleanup_winsock();
            return -1;
        }

#else
        // Unix/Linux: Use fork-based approach for concurrent server and browser
        // Create a pipe for IPC
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            cleanup_winsock();
            return -1;
        }

        // Create a separate process for the server
        pid_t server_pid = fork();
        if (server_pid == 0) {
            // Child process - run the server
            close(pipefd[0]); // Close read end in child

            char temp_auth_code[256] = {0};
            int result = start_local_server(temp_auth_code, sizeof(temp_auth_code), auth_url, 0);

            if (result == 0 && strlen(temp_auth_code) > 0) {
                // Send the auth code to parent via pipe
                write(pipefd[1], temp_auth_code, strlen(temp_auth_code));
            }

            close(pipefd[1]);
            exit(result);
        } else if (server_pid > 0) {
            // Parent process - wait for server to start, then open browser
            close(pipefd[1]); // Close write end in parent

            // Give child process time to bind and listen on port 8080
            cdrive_usleep(150000);

            print_colored("\n[>] ", COLOR_GREEN);
            printf("Opening browser...\n");
            int browser_result = open_browser_url(auth_url);

            if (browser_result != 0) {
                print_warning("Could not automatically open browser. Please copy the URL above and paste it into your browser manually.");
            }

            // Start spinner for waiting
            LoadingSpinner auth_spinner = {0};
            start_spinner(&auth_spinner, "Waiting for authentication callback...");

            // Wait for the child process to complete (with 5-minute timeout)
            int status;
            pid_t wait_result;
            time_t wait_start = time(NULL);
            while (1) {
                wait_result = waitpid(server_pid, &status, WNOHANG);
                if (wait_result == server_pid) break;
                if (wait_result == -1) {
                    if (errno == EINTR) continue;
                    break;
                }
                if (difftime(time(NULL), wait_start) > 300.0) {
                    kill(server_pid, SIGTERM);
                    waitpid(server_pid, &status, 0);
                    print_error("Authentication timed out after 5 minutes.");
                    close(pipefd[0]);
                    stop_spinner(&auth_spinner);
                    cleanup_winsock();
                    return -1;
                }
                sleep(1);
            }

            stop_spinner(&auth_spinner);

            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                print_error("Failed to receive authorization callback");
                close(pipefd[0]);
                cleanup_winsock();
                return -1;
            }

            // Read the auth code from the pipe
            ssize_t bytes_read = read(pipefd[0], auth_code, sizeof(auth_code) - 1);
            close(pipefd[0]);

            if (bytes_read <= 0) {
                print_error("Failed to receive authorization code from server process");
                cleanup_winsock();
                return -1;
            }

            auth_code[bytes_read] = '\0';
        } else {
            // Fork failed, fallback to original method
            close(pipefd[0]);
            close(pipefd[1]);
            print_warning("Could not fork process, using fallback method");

            print_colored("\n[>] ", COLOR_GREEN);
            printf("Opening browser...\n");
            open_browser_url(auth_url);

            // Start spinner for waiting
            LoadingSpinner auth_spinner = {0};
            start_spinner(&auth_spinner, "Waiting for authentication callback...");

            // Start local server to receive callback without re-opening browser
            if (start_local_server(auth_code, sizeof(auth_code), auth_url, 0) != 0) {
                stop_spinner(&auth_spinner);
                print_error("Failed to receive authorization callback");
                cleanup_winsock();
                return -1;
            }

            stop_spinner(&auth_spinner);
        }
#endif

        cleanup_winsock();
    }

    if (strlen(auth_code) == 0) {
        print_error("Authorization code is empty");
        return -1;
    }

    print_success("Authorization code received");

    // Exchange code for tokens
    if (exchange_code_for_tokens(auth_code, &tokens) != 0) {
        print_error("Failed to exchange authorization code for tokens");
        return -1;
    }

    // Save tokens
    if (save_tokens(&tokens) != 0) {
        print_error("Failed to save tokens");
        return -1;
    }

    // Update global tokens
    g_tokens = tokens;

    return 0;
}


int save_tokens(const OAuthTokens *tokens) {
    char token_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    if (!home_dir) {
        print_error("Unable to determine home directory");
        return -1;
    }
    
    snprintf(token_path, sizeof(token_path), "%s%s%s%s%s", home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, TOKEN_FILE);
    
#ifdef _WIN32
    FILE *file = fopen(token_path, "w");
#else
    int fd = open(token_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    FILE *file = (fd >= 0) ? fdopen(fd, "w") : NULL;
#endif
    if (!file) {
        perror("Error saving tokens");
        return -1;
    }
    
    json_object *tok_obj = json_object_new_object();
    json_object_object_add(tok_obj, "access_token", json_object_new_string(tokens->access_token));
    json_object_object_add(tok_obj, "refresh_token", json_object_new_string(tokens->refresh_token));
    json_object_object_add(tok_obj, "token_type", json_object_new_string(tokens->token_type));
    json_object_object_add(tok_obj, "expires_in", json_object_new_int(tokens->expires_in));

    fprintf(file, "%s\n", json_object_to_json_string_ext(tok_obj, JSON_C_TO_STRING_PRETTY));
    json_object_put(tok_obj);
    
    if (fclose(file) != 0) {
        perror("Error saving tokens");
        return -1;
    }
    return 0;
}

int load_tokens(OAuthTokens *tokens) {
    char token_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    if (!home_dir) return -1;
    
    snprintf(token_path, sizeof(token_path), "%s%s%s%s%s", home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, TOKEN_FILE);
    
    FILE *file = fopen(token_path, "r");
    if (!file) {
        return -1;
    }
    
    char buffer[2048];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[bytes_read] = '\0';
    
    json_object *root = json_tokener_parse(buffer);
    if (!root) {
        return -1;
    }
    
    json_object *access_token_obj, *refresh_token_obj, *token_type_obj, *expires_in_obj;
    
    if (json_object_object_get_ex(root, "access_token", &access_token_obj)) {
        strncpy(tokens->access_token, json_object_get_string(access_token_obj), 
                sizeof(tokens->access_token) - 1);
    }
    
    if (json_object_object_get_ex(root, "refresh_token", &refresh_token_obj)) {
        strncpy(tokens->refresh_token, json_object_get_string(refresh_token_obj), 
                sizeof(tokens->refresh_token) - 1);
    }
    
    if (json_object_object_get_ex(root, "token_type", &token_type_obj)) {
        strncpy(tokens->token_type, json_object_get_string(token_type_obj), 
                sizeof(tokens->token_type) - 1);
    }
    
    if (json_object_object_get_ex(root, "expires_in", &expires_in_obj)) {
        tokens->expires_in = json_object_get_int(expires_in_obj);
    }
    
    json_object_put(root);
    return 0;
}

int get_user_info(char *user_name, size_t name_size) {
    APIResponse response = {0};

    if (strlen(g_tokens.access_token) == 0) {
        return -1;
    }

    if (cdrive_api_get("https://www.googleapis.com/drive/v3/about?fields=user(displayName,emailAddress)", &response) != 0) {
        return -1;
    }

    // Parse response to get user name
    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *user_obj, *display_name_obj, *email_obj;
            if (json_object_object_get_ex(root, "user", &user_obj)) {
                const char *name = NULL;
                if (json_object_object_get_ex(user_obj, "displayName", &display_name_obj)) {
                    name = json_object_get_string(display_name_obj);
                }
                if ((!name || !*name) && json_object_object_get_ex(user_obj, "emailAddress", &email_obj)) {
                    name = json_object_get_string(email_obj);
                }
                if (name && *name) {
                    strncpy(user_name, name, name_size - 1);
                    user_name[name_size - 1] = '\0';
                    json_object_put(root);
                    free(response.data);
                    return 0;
                }
            }
            json_object_put(root);
        }
        free(response.data);
    }
    
    return -1;
}

int cdrive_auth_logout(void) {
    char token_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    if (!home_dir) {
        print_error("Unable to determine home directory");
        return -1;
    }
    snprintf(token_path, sizeof(token_path), "%s%s%s%s%s", home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, TOKEN_FILE);
    
    // Scrub sensitive tokens from memory
    memset(&g_tokens, 0, sizeof(g_tokens));

    if (unlink(token_path) != 0 && errno != ENOENT) {
        perror("Error removing token file");
        return -1;
    }

    if (g_json_mode) {
        printf("{\"status\":\"success\",\"message\":\"Logged out successfully\"}\n");
    } else {
        print_success("Successfully logged out from Google Drive.");
    }
    return 0;
}