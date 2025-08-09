#ifndef CDRIVE_H
#define CDRIVE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <unistd.h>
#include <sys/stat.h>
#include <termios.h>

// Constants
#define MAX_URL_SIZE 2048
#define MAX_TOKEN_SIZE 1024
#define MAX_PATH_SIZE 512
#define MAX_RESPONSE_SIZE 8192
#define CONFIG_DIR ".cdrive"
#define TOKEN_FILE "token.json"
#define CLIENT_ID_FILE "client_id.json"

// Colors for terminal output
#define COLOR_RESET     "\033[0m"
#define COLOR_BOLD      "\033[1m"
#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_BLUE      "\033[34m"
#define COLOR_MAGENTA   "\033[35m"
#define COLOR_CYAN      "\033[36m"
#define COLOR_WHITE     "\033[37m"
#define COLOR_BG_GREEN  "\033[42m"
#define COLOR_BG_BLUE   "\033[44m"

// OAuth2 Configuration
#define OAUTH_AUTH_URL "https://accounts.google.com/o/oauth2/v2/auth"
#define OAUTH_TOKEN_URL "https://oauth2.googleapis.com/token"
#define DRIVE_API_URL "https://www.googleapis.com/drive/v3/files"
#define UPLOAD_API_URL "https://www.googleapis.com/upload/drive/v3/files"
#define REDIRECT_URI "http://localhost:8080/callback"
#define SCOPE "https://www.googleapis.com/auth/drive.file"

// Menu options
typedef enum {
    MENU_CREDENTIALS = 0,
    MENU_HELP = 1,
    MENU_EXIT = 2
} MenuOption;

// Structures
typedef struct {
    char *data;
    size_t size;
} APIResponse;

typedef struct {
    char access_token[MAX_TOKEN_SIZE];
    char refresh_token[MAX_TOKEN_SIZE];
    char token_type[64];
    int expires_in;
} OAuthTokens;

typedef struct {
    char client_id[256];
    char client_secret[256];
} ClientCredentials;

typedef struct {
    char id[64];
    char name[256];
    char webViewLink[MAX_URL_SIZE];
    char webContentLink[MAX_URL_SIZE];
} DriveFile;

// Global variables (declared in main.c)
extern ClientCredentials g_client_creds;
extern OAuthTokens g_tokens;
extern char g_last_upload_link[MAX_URL_SIZE];

// Function declarations
int cdrive_auth_login(void);
int cdrive_upload(const char *source_path, const char *target_folder);
int cdrive_list_files(const char *folder_id);
int cdrive_create_folder(const char *folder_name, const char *parent_id);

// Helper functions
int setup_config_dir(void);
int save_tokens(const OAuthTokens *tokens);
int load_tokens(OAuthTokens *tokens);
int load_client_credentials(ClientCredentials *creds);
int refresh_access_token(OAuthTokens *tokens);
char *get_file_mime_type(const char *filename);
size_t write_response_callback(void *contents, size_t size, size_t nmemb, APIResponse *response);
void print_usage(void);
void print_version(void);
int start_local_server(char *auth_code);
char *url_encode(const char *str);

// Interactive UI functions
int show_interactive_menu(const char *title, const char **options, int num_options);
void print_colored(const char *text, const char *color);
void print_header(const char *title);
void print_success(const char *message);
void print_error(const char *message);
void print_info(const char *message);
void print_warning(const char *message);
int get_single_char(void);
void enable_raw_mode(void);
void disable_raw_mode(void);
void clear_line(void);
void move_cursor_up(int lines);

#endif // cdrive_H
