#include <stdio.h>
#include <string.h>

#include <yvex/yvex.h>

static void print_usage(FILE *fp)
{
    fprintf(fp, "usage: yvex <command> [options]\n");
    fprintf(fp, "\n");
    fprintf(fp, "commands:\n");
    fprintf(fp, "  info          show A0 runtime skeleton status\n");
    fprintf(fp, "  help          show help\n");
    fprintf(fp, "  version       show version\n");
    fprintf(fp, "\n");
    fprintf(fp, "options:\n");
    fprintf(fp, "  --help        show help\n");
    fprintf(fp, "  --version     show version\n");
}

static void print_info(void)
{
    printf("name: YVEX\n");
    printf("version: %s\n", yvex_version_string());
    printf("language: C\n");
    printf("interface: CLI-only\n");
    printf("status: A0 core/CLI skeleton\n");
    printf("inference: not implemented\n");
    printf("gguf: not implemented\n");
    printf("cuda: not implemented\n");
    printf("server: not implemented\n");
}

static int command_help(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[2], "info") == 0) {
        printf("usage: yvex info\n");
        printf("\n");
        printf("Print the implemented A0 core/CLI skeleton status.\n");
        return 0;
    }

    print_usage(stdout);
    return 0;
}

int main(int argc, char **argv)
{
    const char *cmd;

    if (argc == 1) {
        print_usage(stdout);
        return 0;
    }

    cmd = argv[1];

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(stdout);
        return 0;
    }

    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "version") == 0) {
        printf("yvex %s\n", yvex_version_string());
        return 0;
    }

    if (strcmp(cmd, "help") == 0) {
        return command_help(argc, argv);
    }

    if (strcmp(cmd, "info") == 0) {
        print_info();
        return 0;
    }

    fprintf(stderr, "yvex: unknown command '%s'\n", cmd);
    fprintf(stderr, "try 'yvex --help'\n");
    return 2;
}
