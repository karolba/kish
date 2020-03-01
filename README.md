# `kish` - a simple shell, aspiring to be fully POSIX-compatibile in the future

### What currently works:
- simple commands
- quoting
- redirections (`> file`)
- piping (`command1 | command2`)
- conditional execution: `&&` and `||`
- compound commands (`{ command1; command2 } | command3`)
- builitins:
  - `true`
  - `false`
- piping and redirecting to/from bulitins without spawning a subshell
- variables:
  - return value from last command - `$?`
  - current pid - `$$`
- inline environment variables (`HOME='/' command`)
