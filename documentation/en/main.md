## Install

Download 'theo.exe' from [releases](https://github.com/Theodikes/theo-bases-soft/releases) and run executable file. Now you can use soft via console from any directory.

**NB:** After first run, don`t move, delete or rename execution file, because then program won't run from console.



## Usage

`theo command [command option(s)] path(s) `:

1. `theo` - program name
2. `command` - short or full command (`d` or `deduplicate` to remove duplicates from file, `c` or `count` to count the number of lines in a file and so on)
3. `command options(s)` - unique option for each command (e.g. `-l` or `--lines` for `split` command - how many lines should be in each file after splitting)
4. `path(s)` - paths to files (or folders, if you need to process all files in a folder) to which the command is applied

For example: `theo d -d dedup.txt test.txt` - program reads all lines from **test.txt**, removes duplicates, and saves all unique lines to **dedup.txt** file. File **test.txt** remains unchanged on disk.

Use `theo -h` to get all commands list and use `theo [command] -h` to get all options for specified command.