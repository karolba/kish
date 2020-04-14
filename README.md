# `kish` - a simple shell, aspiring to be fully POSIX-compatibile in the future

## What currently works:
- simple commands
- quoting
- redirections (`> file`)
- piping (`command1 | command2`)
- conditional execution: `&&` and `||`
- compound commands (`{ command1; command2 } | command3`)
- builitins:
  - `true`
  - `false`
  - `cd` (without `-P` and `-L`)
- if statements: `if <command-list>; then <command-list>; [else <command-list>]; fi`
- piping and redirecting to/from bulitins/command lists/if statements
- variables:
  - return value from last command - `$?`
  - current pid - `$$`
- inline environment variables (`HOME='/' command`)

## Building

The `kish` shell depends on [tcbrindle/span](https://github.com/tcbrindle/span).

```sh
$ git submodule update --init
```

```sh
$ cmake .
$ make
```


## Testing

The `kish` shell can be tested by a shell script located in `test/tests.sh`.
Simply run the shellscript with a path to built `kish` as its argument.
