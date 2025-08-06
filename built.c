#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define OS_NAME "Windows"
#define EXE_EXT ".exe"
#define OBJ_EXT ".obj"
#define PATH_SEP "\\"
#else
#include <unistd.h>
#include <sys/types.h>
#define OS_NAME "Unix/Linux/Mac"
#define EXE_EXT ""
#define OBJ_EXT ".o"
#define PATH_SEP "/"
#endif

typedef struct
{
    const char *source;
    const char *object;
} FileInfo;

// 執行系統命令
int run_command(const char *cmd)
{
    printf("$ %s\n", cmd);
    int result = system(cmd);
    if (result != 0)
    {
        printf("Error: Command failed with code %d\n", result);
    }
    return result;
}

// 檢查檔案是否存在
int file_exists(const char *filename)
{
    struct stat st;
    return stat(filename, &st) == 0;
}

// 建立目錄
void create_directory(const char *path)
{
    if (!file_exists(path))
    {
#ifdef _WIN32
        _mkdir(path);
#else
        mkdir(path, 0755);
#endif
        printf("Created directory: %s\n", path);
    }
}

// 建立範例 index.html
void create_sample_html()
{
    const char *html_content =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <title>Welcome to C Web Server</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; text-align: center; padding: 50px; }\n"
        "        h1 { color: #333; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Welcome to C Web Server!</h1>\n"
        "    <p>Your server is running successfully.</p>\n"
        "    <p>Time: <span id=\"time\"></span></p>\n"
        "    <script>\n"
        "        document.getElementById('time').textContent = new Date().toLocaleString();\n"
        "    </script>\n"
        "</body>\n"
        "</html>\n";

    FILE *file = fopen("www" PATH_SEP "index.html", "w");
    if (file)
    {
        fwrite(html_content, 1, strlen(html_content), file);
        fclose(file);
        printf("Created sample index.html\n");
    }
}

// 主要編譯函數
int build_project(const char *mode)
{
    // 判斷編譯模式
    int is_framework = (mode && strcmp(mode, "framework") == 0);
    int is_clean = (mode && strcmp(mode, "clean") == 0);

// 設定編譯器和參數
#ifdef _WIN32
    const char *cc = "gcc";
    const char *cflags = "-Wall -Wextra -O2 -I. -Icore -Istatic_server -Iapi_framework";
    const char *ldflags = "-lws2_32";
    const char *static_target = "webserver.exe";
    const char *framework_target = "webapi.exe";
#else
    const char *cc = "gcc";
    const char *cflags = "-Wall -Wextra -O2 -I. -Icore -Istatic_server -Iapi_framework";
    const char *ldflags = "-pthread";
    const char *static_target = "webserver";
    const char *framework_target = "webapi";
#endif

    // 清理模式
    if (is_clean)
    {
        printf("Cleaning build files...\n");

        // 刪除所有 .o 檔案
        const char *objects[] = {
            "static_server" OBJ_EXT,
            "server" OBJ_EXT,
            "http_handler_static" OBJ_EXT,
            "http_handler_api" OBJ_EXT,
            "file_utils" OBJ_EXT,
            "logger" OBJ_EXT,
            "router" OBJ_EXT,
            "json" OBJ_EXT,
            "example_app" OBJ_EXT};

        for (int i = 0; i < sizeof(objects) / sizeof(objects[0]); i++)
        {
            if (file_exists(objects[i]))
            {
                remove(objects[i]);
                printf("Removed %s\n", objects[i]);
            }
        }

        // 刪除執行檔
        if (file_exists(static_target))
        {
            remove(static_target);
            printf("Removed %s\n", static_target);
        }
        if (file_exists(framework_target))
        {
            remove(framework_target);
            printf("Removed %s\n", framework_target);
        }
        if (file_exists("server.log"))
        {
            remove("server.log");
            printf("Removed server.log\n");
        }

        printf("Clean complete!\n");
        return 0;
    }

    printf("Building C Web Server (%s mode) for %s...\n\n",
           is_framework ? "framework" : "static", OS_NAME);

    // 檢查編譯器
    char check_cmd[256];
    sprintf(check_cmd, "%s --version > %s 2>&1", cc,
#ifdef _WIN32
            "nul"
#else
            "/dev/null"
#endif
    );

    if (system(check_cmd) != 0)
    {
        printf("Error: %s not found. Please install GCC.\n", cc);
        return 1;
    }

    char cmd[1024];

    if (is_framework)
    {
        // Framework 模式 - 編譯 API 伺服器
        FileInfo files[] = {
            {"core" PATH_SEP "server.c", "server" OBJ_EXT},
            {"api_framework" PATH_SEP "http_handler_api.c", "http_handler_api" OBJ_EXT},
            {"core" PATH_SEP "file_utils.c", "file_utils" OBJ_EXT},
            {"core" PATH_SEP "logger.c", "logger" OBJ_EXT},
            {"api_framework" PATH_SEP "router.c", "router" OBJ_EXT},
            {"api_framework" PATH_SEP "json.c", "json" OBJ_EXT},
            {"api_framework" PATH_SEP "example_app.c", "example_app" OBJ_EXT}};
        int file_count = sizeof(files) / sizeof(files[0]);

        // 檢查必要檔案
        printf("Checking required files for framework build...\n");
        int missing = 0;
        for (int i = 0; i < file_count; i++)
        {
            if (!file_exists(files[i].source))
            {
                printf("Missing: %s\n", files[i].source);
                missing = 1;
            }
        }

        if (missing)
        {
            printf("\nError: Required files are missing for framework build.\n");
            printf("Please ensure all framework files are present.\n");
            return 1;
        }

        // 編譯每個源檔案
        for (int i = 0; i < file_count; i++)
        {
            printf("Compiling %s...\n", files[i].source);
            sprintf(cmd, "%s %s -c %s -o %s", cc, cflags, files[i].source, files[i].object);
            if (run_command(cmd) != 0)
                return 1;
        }

        // 連結
        printf("\nLinking framework...\n");
        sprintf(cmd, "%s", cc);
        for (int i = 0; i < file_count; i++)
        {
            strcat(cmd, " ");
            strcat(cmd, files[i].object);
        }
        strcat(cmd, " -o ");
        strcat(cmd, framework_target);
        strcat(cmd, " ");
        strcat(cmd, ldflags);

        if (run_command(cmd) != 0)
            return 1;

        // 清理中間檔案
        printf("\nCleaning intermediate files...\n");
        for (int i = 0; i < file_count; i++)
        {
            if (file_exists(files[i].object))
            {
                remove(files[i].object);
                printf("Removed %s\n", files[i].object);
            }
        }

        printf("\n========================================\n");
        printf("Framework build successful!\n");
        printf("Executable: %s\n", framework_target);
        printf("========================================\n");
        printf("\nUsage: .%s%s [port]\n", PATH_SEP, framework_target);
        printf("Default port: 8080\n");
    }
    else
    {
        // Static 模式 - 編譯靜態檔案伺服器
        FileInfo files[] = {
            {"static_server" PATH_SEP "static_server.c", "static_server" OBJ_EXT},
            {"core" PATH_SEP "server.c", "server" OBJ_EXT},
            {"static_server" PATH_SEP "http_handler_static.c", "http_handler_static" OBJ_EXT},
            {"core" PATH_SEP "file_utils.c", "file_utils" OBJ_EXT},
            {"core" PATH_SEP "logger.c", "logger" OBJ_EXT}};
        int file_count = sizeof(files) / sizeof(files[0]);

        // 檢查必要檔案
        printf("Checking required files for static server build...\n");
        int missing = 0;
        for (int i = 0; i < file_count; i++)
        {
            if (!file_exists(files[i].source))
            {
                printf("Missing: %s\n", files[i].source);
                missing = 1;
            }
        }

        if (missing)
        {
            printf("\nError: Required files are missing for static server build.\n");
            printf("Please ensure all files are in the correct folders:\n");
            printf("- Core files in 'core' folder\n");
            printf("- Static server files in 'static_server' folder\n");
            return 1;
        }

        // 編譯每個源檔案
        for (int i = 0; i < file_count; i++)
        {
            printf("Compiling %s...\n", files[i].source);
            sprintf(cmd, "%s %s -c %s -o %s", cc, cflags, files[i].source, files[i].object);
            if (run_command(cmd) != 0)
                return 1;
        }

        // 連結
        printf("\nLinking...\n");
        sprintf(cmd, "%s", cc);
        for (int i = 0; i < file_count; i++)
        {
            strcat(cmd, " ");
            strcat(cmd, files[i].object);
        }
        strcat(cmd, " -o ");
        strcat(cmd, static_target);
        strcat(cmd, " ");
        strcat(cmd, ldflags);

        if (run_command(cmd) != 0)
            return 1;

        // 清理中間檔案
        printf("\nCleaning intermediate files...\n");
        for (int i = 0; i < file_count; i++)
        {
            if (file_exists(files[i].object))
            {
                remove(files[i].object);
                printf("Removed %s\n", files[i].object);
            }
        }

        // 建立 www 目錄
        create_directory("www");

        // 建立範例 HTML
        if (!file_exists("www" PATH_SEP "index.html"))
        {
            create_sample_html();
        }

        printf("\n========================================\n");
        printf("Static server build successful!\n");
        printf("Executable: %s\n", static_target);
        printf("========================================\n");
        printf("\nUsage: .%s%s [port]\n", PATH_SEP, static_target);
        printf("Default port: 8080\n");
    }

    return 0;
}

void print_usage()
{
    printf("C Web Server Build Tool\n");
    printf("=======================\n");
    printf("Usage:\n");
    printf("  build              - Build static file server\n");
    printf("  build framework    - Build web API framework\n");
    printf("  build clean        - Clean all build files\n");
    printf("  build help         - Show this help\n");
    printf("\n");
    printf("Expected folder structure:\n");
    printf("  core/              - Core modules (server, logger, file_utils)\n");
    printf("  static_server/     - Static server files (static_server, http_handler_static)\n");
    printf("  api_framework/     - API framework files (example_app, http_handler_api, router, json)\n");
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "help") == 0)
        {
            print_usage();
            return 0;
        }
        return build_project(argv[1]);
    }

    // 預設編譯靜態伺服器
    return build_project(NULL);
}