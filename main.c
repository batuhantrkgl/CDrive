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

            int headless = 0;
            if (argc > 3 && strcmp(argv[3], "--no-browser") == 0) {
                headless = 1;
            }

            if (cdrive_auth_login(headless) == 0) {
                printf("\n");
                print_success("Authentication complete.");
                print_success("Configured Google Drive access");
                
                // Get user info and display personalized message
                char user_name[256] = {0};
                if (get_user_info(user_name, sizeof(user_name)) == 0) {
                    printf("%s[+]%s Logged in as %s%s%s on Google Drive.\n", 
                           COLOR_GREEN, COLOR_RESET, COLOR_BOLD, user_name, COLOR_RESET);
                } else {
                    print_success("Logged in to Google Drive");
                }
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
        print_colored("[>] ", COLOR_BLUE);
        printf("Listing files in folder: %s\n", folder_id);
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
        
        print_colored("[>] ", COLOR_BLUE);
        printf("Creating folder '%s'...\n", folder_name);
        if (cdrive_create_folder(folder_name, parent_id) != 0) {
            print_error("Failed to create folder.");
            curl_global_cleanup();
            return 1;
        }
    } else if (strcmp(argv[1], "pull") == 0) {
        if (load_tokens(&g_tokens) != 0) {
            print_error("Not authenticated. Run 'cdrive auth login' first.");
            curl_global_cleanup();
            return 1;
        }
        if (argc > 2) {
            // Direct download by ID
            const char *file_id = argv[2];
            const char *output_filename = (argc > 3) ? argv[3] : NULL; // Pass NULL if not provided
            if (cdrive_pull_file_by_id(file_id, output_filename) != 0) {
                // Error is printed inside the function
                curl_global_cleanup();
                return 1;
            }
        } else {
            // Interactive download
            cdrive_pull_interactive();
        }
    } else if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0) {
        print_version_with_update_check();
    } else if (strcmp(argv[1], "update") == 0) {
        if (argc < 3) {
            print_colored("Usage: ", COLOR_BOLD);
            printf("%s update <option>\n\n", argv[0]);
            print_colored("UPDATE OPTIONS\n", COLOR_BOLD);
            printf("  --auto     Download and install pre-compiled binary automatically\n");
            printf("  --compile  Download source and compile on your machine\n");
            printf("  --check    Check for updates without installing\n");
            curl_global_cleanup();
            return 1;
        }
        
        if (strcmp(argv[2], "--check") == 0) {
            printf("\n");
            print_colored("[*] ", COLOR_YELLOW);
            printf("Forcing update check (bypassing cache)...\n");
            
            UpdateInfo update_info = {0};
            int update_result = force_check_for_updates(&update_info);
            
            if (update_result == 0) {
                if (update_info.is_newer) {
                    printf("\n");
                    print_colored("[+] ", COLOR_GREEN);
                    print_colored("Update Available! ", COLOR_BOLD);
                    printf("Version %s", update_info.version);
                    if (strlen(update_info.release_date) > 0) {
                        printf(" (%s)", update_info.release_date);
                    }
                    printf("\n");
                    
                    print_colored("[*] ", COLOR_CYAN);
                    printf("Run ");
                    print_colored("cdrive update --auto", COLOR_YELLOW);
                    printf(" to install pre-compiled binary\n");
                    
                    print_colored("[*] ", COLOR_CYAN);
                    printf("Or run ");
                    print_colored("cdrive update --compile", COLOR_YELLOW);
                    printf(" to automatically compile it on your machine\n");
                } else {
                    printf("\n");
                    print_colored("[+] ", COLOR_GREEN);
                    printf("You're running the latest version!\n");
                }
            } else if (update_result == -2) {
                printf("\n");
                print_colored("[!] ", COLOR_YELLOW);
                printf("GitHub API rate limit exceeded. Try again later.\n");
            } else if (update_result == -3) {
                printf("\n");
                print_colored("[!] ", COLOR_RED);
                printf("Repository not found or releases not available.\n");
            } else {
                printf("\n");
                print_colored("[!] ", COLOR_YELLOW);
                printf("Could not check for updates. Please check your internet connection.\n");
            }
        } else if (strcmp(argv[2], "--auto") == 0) {
            printf("\n");
            print_colored("[*] ", COLOR_YELLOW);
            printf("Checking for updates...\n");
            
            UpdateInfo update_info = {0};
            if (force_check_for_updates(&update_info) == 0) {
                if (update_info.is_newer) {
                    printf("\n");
                    print_colored("[+] ", COLOR_GREEN);
                    printf("Update available: %s -> %s\n", CDRIVE_VERSION, update_info.version);
                    
                    int install_result = download_and_install_update(&update_info, 1);
                    if (install_result == 0) {
                        printf("\n");
                        print_success("Update completed successfully!");
                    } else {
                        if (install_result == -1) {
                            printf("\n");
                            print_warning("Update download succeeded, but installation requires manual steps");
                        } else {
                            print_error("Update failed");
                            curl_global_cleanup();
                            return 1;
                        }
                    }
                } else {
                    printf("\n");
                    print_success("You're already running the latest version!");
                }
            } else {
                print_error("Failed to check for updates");
                curl_global_cleanup();
                return 1;
            }
        } else if (strcmp(argv[2], "--compile") == 0) {
            printf("\n");
            print_colored("[*] ", COLOR_YELLOW);
            printf("Checking for updates...\n");
            
            UpdateInfo update_info = {0};
            if (force_check_for_updates(&update_info) == 0) {
                if (update_info.is_newer) {
                    printf("\n");
                    print_colored("[+] ", COLOR_GREEN);
                    printf("Update available: %s -> %s\n", CDRIVE_VERSION, update_info.version);
                    
                    print_colored("[*] ", COLOR_CYAN);
                    printf("To compile from source:\n");
                    printf("1. git clone https://github.com/batuhantrkgl/CDrive.git\n");
                    printf("2. cd CDrive\n");
                    printf("3. git checkout %s\n", update_info.tag_name);
                    printf("4. make clean && make\n");
                    printf("5. sudo make install\n\n");
                    
                    print_colored("[*] ", COLOR_BLUE);
                    printf("Or download source archive:\n");
                    printf("   %s/archive/%s.tar.gz\n", 
                           GITHUB_RELEASES_URL, update_info.tag_name);
                } else {
                    printf("\n");
                    print_success("You're already running the latest version!");
                }
            } else {
                print_error("Failed to check for updates");
                curl_global_cleanup();
                return 1;
            }
        } else {
            print_error("Unknown update option");
            printf("Run 'cdrive update --help' for usage.\n");
            curl_global_cleanup();
            return 1;
        }
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
    printf("\n");
    print_colored("cdrive", COLOR_BOLD_GREEN);
    printf(" - A professional, lightweight command-line interface for Google Drive.\n\n");
    
    print_colored("USAGE\n", COLOR_BOLD);
    printf("  cdrive <command> [subcommand] [arguments]\n\n");
    
    print_colored("CORE COMMANDS\n", COLOR_BOLD);
    printf("  %sauth%s        Manage authentication with Google Drive\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %supload%s      Upload a file to a specific folder\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %slist%s        List files and folders\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %smkdir%s       Create a new folder\n\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %spull%s        Download a file or browse interactively\n\n", COLOR_YELLOW, COLOR_RESET);
    
    print_colored("ADDITIONAL COMMANDS\n", COLOR_BOLD);
    printf("  %sversion%s     Show version information and check for updates\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %supdate%s      Update cdrive to the latest version\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %shelp%s        Show this help message\n\n", COLOR_YELLOW, COLOR_RESET);
    
    print_colored("EXAMPLES\n", COLOR_BOLD);
    printf("  %s# Authenticate with your Google account%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  $ cdrive auth login\n\n");
    printf("  %s# Upload a file to the root folder%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  $ cdrive upload ./document.pdf\n\n");
    printf("  %s# List files in a specific folder%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  $ cdrive list 1BxiMVs...pU\n\n");
    printf("  %s# Download a file by its ID (filename is fetched automatically)%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  $ cdrive pull 1BxiMVs...pU\n\n");
    printf("  %s# Browse files interactively to download%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  $ cdrive pull\n\n");
    
    print_colored("LEARN MORE\n", COLOR_BOLD);
    printf("  Use 'cdrive <command>' for more information about a command.\n");
    printf("  Find the source code at: %s%s%s\n\n", COLOR_BLUE, GITHUB_RELEASES_URL, COLOR_RESET);
}
