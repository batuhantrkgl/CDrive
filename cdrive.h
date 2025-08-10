#ifndef CDRIVE_H
#define CDRIVE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <conio.h>
    #include <direct.h>
    #include <io.h>
    #include <sys/stat.h>
    #define mkdir(path, mode) _mkdir(path)
    #define access(path, mode) _access(path, mode)
    #define F_OK 0
    #define PATH_SEP "\\"
    #define HOME_ENV "USERPROFILE"
    #define STDIN_FILENO 0
    #define close(fd) closesocket(fd)
    #define read(fd, buf, len) recv(fd, buf, len, 0)
    #define write(fd, buf, len) send(fd, buf, len, 0)
    typedef int socklen_t;
    
    // Initialize Winsock
    static int init_winsock(void) {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2,2), &wsaData);
    }
    
    static void cleanup_winsock(void) {
        WSACleanup();
    }
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <termios.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #define PATH_SEP "/"
    #define HOME_ENV "HOME"
    
    static int init_winsock(void) { return 0; }
    static void cleanup_winsock(void) { }
#endif

// Cross-platform terminal functions
#ifdef _WIN32
    // Windows console handle
    static HANDLE hConsole = INVALID_HANDLE_VALUE;
    static DWORD dwOriginalMode = 0;
    
    static void init_console(void) {
        if (hConsole == INVALID_HANDLE_VALUE) {
            hConsole = GetStdHandle(STD_INPUT_HANDLE);
            GetConsoleMode(hConsole, &dwOriginalMode);
        }
    }
    
    static void enable_raw_mode(void) {
        init_console();
        DWORD dwMode = dwOriginalMode;
        dwMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
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
#else
    // Unix/Linux termios functions
    static struct termios orig_termios;
    
    static void enable_raw_mode(void) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    
    static void disable_raw_mode(void) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
    
    static int platform_getchar(void) {
        return getchar();
    }
#endif

// Constants
#define MAX_URL_SIZE 2048
#define MAX_TOKEN_SIZE 1024
#define MAX_PATH_SIZE 512
#define MAX_RESPONSE_SIZE 8192
#define CONFIG_DIR ".cdrive"
#define TOKEN_FILE "token.json"
#define CLIENT_ID_FILE "client_id.json"
#define UPDATE_CACHE_FILE "update_cache.json"
#define UPDATE_CACHE_EXPIRE_HOURS 4  // Cache update checks for 4 hours

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

// Version and update information
#define CDRIVE_VERSION "1.0.1"
#define CDRIVE_RELEASE_DATE "2025-08-11"
#define GITHUB_REPO_URL "https://api.github.com/repos/batuhantrkgl/CDrive/releases/latest"
#define GITHUB_RELEASES_URL "https://github.com/batuhantrkgl/CDrive/releases"

typedef struct {
    char version[32];
    char release_date[16];
    char download_url[256];
    char tag_name[32];
    int is_newer;
} UpdateInfo;

// Helper functions
int setup_config_dir(void);
int save_tokens(const OAuthTokens *tokens);
int load_tokens(OAuthTokens *tokens);
int load_client_credentials(ClientCredentials *creds);
int refresh_access_token(OAuthTokens *tokens);
int get_user_info(char *user_name, size_t name_size);
char *get_file_mime_type(const char *filename);
size_t write_response_callback(void *contents, size_t size, size_t nmemb, APIResponse *response);
void print_usage(void);
void print_version(void);
void print_version_with_update_check(void);
int check_for_updates(UpdateInfo *update_info);
int force_check_for_updates(UpdateInfo *update_info);
int download_and_install_update(const UpdateInfo *update_info, int auto_install);
int compare_versions(const char *current, const char *latest);
int start_local_server(char *auth_code, const char *auth_url, int open_browser);
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

// Loading spinner functions
typedef struct {
    int active;
    pthread_t thread;
    char message[256];
} LoadingSpinner;

void start_spinner(LoadingSpinner *spinner, const char *message);
void stop_spinner(LoadingSpinner *spinner);
void *spinner_thread(void *arg);

#endif // cdrive_H
