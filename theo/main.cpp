#include "utils.hpp"
#include "commands.hpp"

static const char* const usages[] = {
    "theo [global options] [command] [command options] [command args]",
    NULL,
};

int main(int argc, const char** argv) {
    struct argparse argparse;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP(commandsDescription),
        OPT_END(),
    };
    argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
    argparse_describe(&argparse, "\nFastest and most efficient database processing soft.", "\nCreated by Theo <Telegram: @just_temp>");
    argc = argparse_parse(&argparse, argc, argv);
    if (argc < 1) {
        argparse_usage(&argparse);
        return -1;
    }

    /* Try to run command with args provided. */
    struct cmd_struct* cmd = NULL;
    for (int i = 0; i < _countof(commands); i++) {
        if (!strcmp(commands[i].cmd, argv[0])) {
            cmd = &commands[i];
        }
    }
    if (cmd) {
        return cmd->fn(argc, argv);
    }
    return 0;
}