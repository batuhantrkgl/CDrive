#define _GNU_SOURCE
#include "cdrive.h"

int show_interactive_menu(const char *question, const char **options, int num_options) {
    int selected = 0;
    char input;
    
    while (1) {
        // Clear screen and position cursor at top
        printf("\033[2J\033[H");
        
        // Display question with colored prompt
        print_colored("[?] ", COLOR_CYAN);
        print_colored(question, COLOR_BOLD);
        printf("\n\n");
        printf("Use %s↑/↓%s arrows to navigate, %sEnter%s to select, %sq%s to quit:\n\n", 
               COLOR_YELLOW, COLOR_RESET, COLOR_YELLOW, COLOR_RESET, COLOR_YELLOW, COLOR_RESET);
        
        // Display options
        for (int i = 0; i < num_options; i++) {
            if (i == selected) {
                print_colored("> ", COLOR_GREEN);
                print_colored(options[i], COLOR_BOLD);
                printf("\n");
            } else {
                printf("  %s\n", options[i]);
            }
        }
        
        printf("\n");
        fflush(stdout);
        
        // Get input
        enable_raw_mode();
        input = platform_getchar();
        
        switch (input) {
            case '\033': // Arrow keys sequence
                if (platform_getchar() == '[') {
                    char arrow = platform_getchar();
                    switch (arrow) {
                        case 'A': // Up arrow
                            selected = (selected - 1 + num_options) % num_options;
                            break;
                        case 'B': // Down arrow
                            selected = (selected + 1) % num_options;
                            break;
                    }
                }
                break;
            case '\n': // Enter
            case '\r':
                disable_raw_mode();
                // Clear screen and show final selection
                printf("\033[2J\033[H");
                print_colored("[>] ", COLOR_GREEN);
                print_colored("Selected: ", COLOR_BOLD);
                printf("%s\n\n", options[selected]);
                return selected;
            case 'q':
            case 'Q':
                disable_raw_mode();
                printf("\033[2J\033[H");
                print_colored("[!] ", COLOR_YELLOW);
                printf("Authentication cancelled.\n");
                return -1;
            case 'k': // vim-style up
            case 'K':
                selected = (selected - 1 + num_options) % num_options;
                break;
            case 'j': // vim-style down
            case 'J':
                selected = (selected + 1) % num_options;
                break;
        }
        disable_raw_mode();
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
    printf("\n%s%s%s\n", COLOR_BOLD, title, COLOR_RESET);
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
    
    // Ask for Client Secret
    printf("%s? Client Secret:%s ", COLOR_CYAN, COLOR_RESET);
    if (!fgets(client_secret, sizeof(client_secret), stdin)) {
        print_error("Failed to read client secret");
        return -1;
    }
    client_secret[strcspn(client_secret, "\n")] = 0; // Remove newline
    
    // Validate input
    if (strlen(client_id) < 10 || strlen(client_secret) < 10) {
        print_error("Invalid credentials. Please check your input.");
        return -1;
    }
    
    // Save to file
    FILE *file = fopen(config_path, "w");
    if (!file) {
        print_error("Error creating credentials file");
        return -1;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"client_id\": \"%s\",\n", client_id);
    fprintf(file, "  \"client_secret\": \"%s\"\n", client_secret);
    fprintf(file, "}\n");
    fclose(file);
    
    print_colored("[+] ", COLOR_GREEN);
    printf("Credentials saved successfully!\n");
    return 0;
}

int load_client_credentials(ClientCredentials *creds) {
    char config_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    
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
    
    json_object *client_id_obj, *client_secret_obj;
    if (!json_object_object_get_ex(root, "client_id", &client_id_obj) ||
        !json_object_object_get_ex(root, "client_secret", &client_secret_obj)) {
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

size_t write_response_callback(void *contents, size_t size, size_t nmemb, APIResponse *response) {
    size_t total_size = size * nmemb;
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

int start_local_server(char *auth_code, const char *auth_url, int open_browser) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
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
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
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
        close(server_fd);
        return -1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }
    
    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    
    print_colored("[*] ", COLOR_YELLOW);
    printf("Starting authentication server...\n");
    
    LoadingSpinner spinner = {0};
    start_spinner(&spinner, "Waiting for authentication callback...");
    
    // Open browser if requested
    if (open_browser) {
        // Temporarily stop spinner for clean browser opening message
        stop_spinner(&spinner);
        
#ifdef _WIN32
        char open_cmd[MAX_URL_SIZE];
        snprintf(open_cmd, sizeof(open_cmd), "start \"\" \"%s\"", auth_url);
#else
        char open_cmd[MAX_URL_SIZE];
        snprintf(open_cmd, sizeof(open_cmd), "xdg-open \"%s\" 2>/dev/null || open \"%s\" 2>/dev/null", 
                 auth_url, auth_url);
#endif
        
        printf("\n");  // Ensure we're on a new line
        print_colored("\n[>] ", COLOR_GREEN);
        printf("Opening browser...\n");
        int browser_result = system(open_cmd);
        
        if (browser_result != 0) {
            print_warning("Could not automatically open browser. Please copy the URL above and paste it into your browser manually.");
        }
        
        // Restart spinner after browser message
        start_spinner(&spinner, "Waiting for authentication callback...");
    }
    
    // Accept connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        stop_spinner(&spinner);
        perror("accept");
        close(server_fd);
        return -1;
    }
    
    stop_spinner(&spinner);
    
    // Read request
    read(new_socket, buffer, 4096);
    
    // Send response
    send(new_socket, response_html, strlen(response_html), 0);
    
    // Parse authorization code from request
    char *code_start = strstr(buffer, "code=");
    if (code_start) {
        code_start += 5; // Skip "code="
        char *code_end = strchr(code_start, '&');
        if (!code_end) code_end = strchr(code_start, ' ');
        if (code_end) {
            size_t code_length = code_end - code_start;
            strncpy(auth_code, code_start, code_length);
            auth_code[code_length] = '\0';
        }
    }
    
    close(new_socket);
    close(server_fd);
    
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

static int exchange_code_for_tokens(const char *auth_code, OAuthTokens *tokens) {
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        print_error("Error initializing curl");
        return -1;
    }
    
    // URL encode the authorization code and redirect URI
    char *encoded_code = url_encode(auth_code);
    char *encoded_redirect = url_encode(REDIRECT_URI);
    
    if (!encoded_code || !encoded_redirect) {
        print_error("Error encoding parameters");
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Prepare POST data
    char post_data[2048];
    snprintf(post_data, sizeof(post_data),
        "code=%s&client_id=%s&client_secret=%s&redirect_uri=%s&grant_type=authorization_code",
        encoded_code, g_client_creds.client_id, g_client_creds.client_secret, encoded_redirect);
    
    free(encoded_code);
    free(encoded_redirect);
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, OAUTH_TOKEN_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
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
        if (response.data) free(response.data);
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
    if (response.data) free(response.data);
    
    return strlen(tokens->access_token) > 0 ? 0 : -1;
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
    char *encoded_redirect = url_encode(REDIRECT_URI);
    char *encoded_scope = url_encode(SCOPE);

    if (!encoded_redirect || !encoded_scope) {
        print_error("Error encoding parameters");
        return -1;
    }

    // Build authorization URL
    snprintf(auth_url, sizeof(auth_url),
        "%s?client_id=%s&redirect_uri=%s&scope=%s&response_type=code&access_type=offline&prompt=consent",
        OAUTH_AUTH_URL, g_client_creds.client_id, encoded_redirect, encoded_scope);

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
            if (code_end) {
                size_t code_length = code_end - code_start;
                strncpy(auth_code, code_start, code_length);
                auth_code[code_length] = '\0';
            } else {
                // if no '&' or ' ' is found, the code is the rest of the string
                strncpy(auth_code, code_start, sizeof(auth_code) - 1);
                auth_code[sizeof(auth_code) - 1] = '\0';
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
        if (start_local_server(auth_code, auth_url, 1) != 0) {
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
            int result = start_local_server(temp_auth_code, auth_url, 0);

            if (result == 0 && strlen(temp_auth_code) > 0) {
                // Send the auth code to parent via pipe
                write(pipefd[1], temp_auth_code, strlen(temp_auth_code));
            }

            close(pipefd[1]);
            exit(result);
        } else if (server_pid > 0) {
            // Parent process - wait a moment for server to start, then open browser
            close(pipefd[1]); // Close write end in parent
            sleep(1); // Give server time to start

            // Open browser
            char open_cmd[MAX_URL_SIZE];
            snprintf(open_cmd, sizeof(open_cmd), "xdg-open \"%s\" 2>/dev/null || open \"%s\" 2>/dev/null",
                     auth_url, auth_url);
            print_colored("\n[>] ", COLOR_GREEN);
            printf("Opening browser...\n");
            int browser_result = system(open_cmd);

            if (browser_result != 0) {
                print_warning("Could not automatically open browser. Please copy the URL above and paste it into your browser manually.");
            }

            // Start spinner for waiting
            LoadingSpinner auth_spinner = {0};
            start_spinner(&auth_spinner, "Waiting for authentication callback...");

            // Wait for the child process to complete
            int status;
            waitpid(server_pid, &status, 0);

            stop_spinner(&auth_spinner);

            if (WEXITSTATUS(status) != 0) {
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

            // Open browser
            char open_cmd[MAX_URL_SIZE];
            snprintf(open_cmd, sizeof(open_cmd), "xdg-open \"%s\" 2>/dev/null || open \"%s\" 2>/dev/null",
                     auth_url, auth_url);
            print_colored("\n[>] ", COLOR_GREEN);
            printf("Opening browser...\n");
            system(open_cmd);

            // Start spinner for waiting
            LoadingSpinner auth_spinner = {0};
            start_spinner(&auth_spinner, "Waiting for authentication callback...");

            // Start local server to receive callback
            if (start_local_server(auth_code, auth_url, 1) != 0) {
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
    
    snprintf(token_path, sizeof(token_path), "%s%s%s%s%s", home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, TOKEN_FILE);
    
    FILE *file = fopen(token_path, "w");
    if (!file) {
        perror("Error saving tokens");
        return -1;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"access_token\": \"%s\",\n", tokens->access_token);
    fprintf(file, "  \"refresh_token\": \"%s\",\n", tokens->refresh_token);
    fprintf(file, "  \"token_type\": \"%s\",\n", tokens->token_type);
    fprintf(file, "  \"expires_in\": %d\n", tokens->expires_in);
    fprintf(file, "}\n");
    
    fclose(file);
    return 0;
}

int load_tokens(OAuthTokens *tokens) {
    char token_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    
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
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};
    
    if (strlen(g_tokens.access_token) == 0) {
        return -1;
    }
    
    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }
    
    // Set up authorization header
    char auth_header[MAX_TOKEN_SIZE];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    
    // Configure curl to get user info from Google Drive API
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/drive/v3/about?fields=user");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        if (response.data) free(response.data);
        return -1;
    }
    
    // Parse response to get user name
    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *user_obj, *display_name_obj;
            
            if (json_object_object_get_ex(root, "user", &user_obj) &&
                json_object_object_get_ex(user_obj, "displayName", &display_name_obj)) {
                
                const char *name = json_object_get_string(display_name_obj);
                strncpy(user_name, name, name_size - 1);
                user_name[name_size - 1] = '\0';
                
                json_object_put(root);
                free(response.data);
                return 0;
            }
            json_object_put(root);
        }
        free(response.data);
    }
    
    return -1;
}
