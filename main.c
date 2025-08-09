#define _GNU_SOURCE
#include "cdrive.h"

// Global variables
ClientCredentials g_client_creds;
OAuthTokens g_tokens;
char g_last_upload_link[MAX_URL_SIZE] = {0};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Setup configuration directory
    if (setup_config_dir() != 0) {
        print_error("Failed to setup configuration directory");
        curl_global_cleanup();
        return 1;
    }

    // Handle commands
    if (strcmp(argv[1], "auth") == 0) {
        if (argc < 3) {
            print_colored("Usage: ", COLOR_BOLD);
            printf("%s auth <command>\n\n", argv[0]);
            print_colored("AUTH COMMANDS\n", COLOR_BOLD);
            printf("  login    Authenticate with Google Drive\n");
            printf("  status   Show authentication status\n");
            curl_global_cleanup();
            return 1;
        }

        if (strcmp(argv[2], "login") == 0) {
            print_header("Google Drive Authentication");
            if (cdrive_auth_login() == 0) {
                printf("\n");
                print_success("Authentication complete.");
                print_success("Configured Google Drive access");
                print_success("Logged in to Google Drive");
            } else {
                print_error("Authentication failed. Please try again.");
                curl_global_cleanup();
                return 1;
            }
        } else if (strcmp(argv[2], "status") == 0) {
            if (load_tokens(&g_tokens) == 0) {
                print_success("Authenticated and ready to use Google Drive");
                if (strlen(g_tokens.access_token) > 20) {
                    printf("Access token: %.20s...\n", g_tokens.access_token);
                } else {
                    printf("Access token: %s\n", g_tokens.access_token);
                }
            } else {
                print_error("Not authenticated. Run 'cdrive auth login' first.");
            }
        } else {
            print_error("Unknown auth command");
            printf("Run 'cdrive auth --help' for usage.\n");
            curl_global_cleanup();
            return 1;
        }
    } else if (strcmp(argv[1], "upload") == 0) {
        if (argc < 3) {
            print_colored("Usage: ", COLOR_BOLD);
            printf("%s upload <source> [target_folder]\n\n", argv[0]);
            print_colored("ARGUMENTS\n", COLOR_BOLD);
            printf("  source         Local file path to upload\n");
            printf("  target_folder  Google Drive folder ID (optional, defaults to root)\n");
            curl_global_cleanup();
            return 1;
        }

        const char *source_path = argv[2];
        const char *target_folder = (argc > 3) ? argv[3] : "root";
        
        if (cdrive_upload(source_path, target_folder) != 0) {
            fprintf(stderr, "upload failed\n");
            curl_global_cleanup();
            return 1;
        }
    } else if (strcmp(argv[1], "list") == 0) {
        const char *folder_id = (argc > 2) ? argv[2] : "root";
        printf("📁 Listing files in folder: %s\n", folder_id);
        cdrive_list_files(folder_id);
    } else if (strcmp(argv[1], "mkdir") == 0) {
        if (argc < 3) {
            print_colored("Usage: ", COLOR_BOLD);
            printf("%s mkdir <folder_name> [parent_folder_id]\n", argv[0]);
            curl_global_cleanup();
            return 1;
        }
        const char *folder_name = argv[2];
        const char *parent_id = (argc > 3) ? argv[3] : "root";
        
        printf("📁 Creating folder '%s'...\n", folder_name);
        if (cdrive_create_folder(folder_name, parent_id) == 0) {
            print_success("Folder created successfully!");
        } else {
            print_error("Failed to create folder.");
            curl_global_cleanup();
            return 1;
        }
    } else if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0) {
        print_version();
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage();
    } else {
        print_error("Unknown command");
        printf("Run 'cdrive --help' for usage.\n");
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}

void print_usage(void) {
    print_colored("cdrive", COLOR_BOLD);
    print_colored(" - Professional Google Drive CLI\n\n", COLOR_RESET);
    
    print_colored("USAGE\n", COLOR_BOLD);
    printf("  cdrive <command> [flags]\n\n");
    
    print_colored("CORE COMMANDS\n", COLOR_BOLD);
    printf("  auth        Authenticate with Google Drive\n");
    printf("  upload      Upload files to Google Drive\n");
    printf("  list        List files in Google Drive\n");
    printf("  mkdir       Create folders in Google Drive\n\n");
    
    print_colored("ADDITIONAL COMMANDS\n", COLOR_BOLD);
    printf("  help        Show help for cdrive\n");
    printf("  version     Show cdrive version\n\n");
    
    print_colored("AUTHENTICATION COMMANDS\n", COLOR_BOLD);
    printf("  auth login  Authenticate with Google Drive\n");
    printf("  auth status Show current authentication status\n\n");
    
    print_colored("EXAMPLES\n", COLOR_BOLD);
    print_colored("  $ ", COLOR_CYAN);
    printf("cdrive auth login\n");
    print_colored("  $ ", COLOR_CYAN);
    printf("cdrive upload ./file.txt\n");
    print_colored("  $ ", COLOR_CYAN);
    printf("cdrive upload ./file.txt parent_folder_id\n");
    print_colored("  $ ", COLOR_CYAN);
    printf("cdrive list\n");
    print_colored("  $ ", COLOR_CYAN);
    printf("cdrive mkdir \"My Folder\"\n\n");
    
    print_colored("LEARN MORE\n", COLOR_BOLD);
    printf("  Use 'cdrive <command> --help' for more information about a command.\n");
}

void print_version(void) {
    print_colored("cdrive", COLOR_BOLD);
    printf(" version ");
    print_colored("1.0.0", COLOR_GREEN);
    printf("\n");
    printf("A professional Google Drive command-line interface\n");
    printf("Built with libcurl and json-c\n");
}
