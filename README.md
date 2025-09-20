# fthis

File Terminal History Interaction Setup

fthis was born from a desire to be able to freely edit shell commands
using a file editor of choice but still have quick command feedback
(i.e., without Ctrl+Z-ing and doing `source the_file`).

## install

do `make install`, checking out the `Makefile` to see what it's doing
under the hood.  if you use vim, consider adding what's in `vimrc`
to your `$HOME/.vimrc`.

## usage

the idea is to do `fthis some_file` to start up a "terminal" in
`some_file` which will execute commands after the file is saved/modified.
you edit `some_file` as desired and any commands after the `# ⬇EXECUTE⬇`
line will be executed on save.  this will will require opening `some_file`
in some other program/terminal, but state will be preserved in the
`fthis` terminal in case you want to check on the state of variables
after some shell commands are run and you Ctrl+C the `fthis` terminal.

try it out with `fthis test.sh` in one terminal, and then edit
`test.sh` in a different window.  you can Ctrl+C the `fthis` terminal
when you're done and continue with any desired commands/state there.

## how it works

under the hood we create a bash function `fthis some_file` to avoid
the more unwieldy `. fthis.sh some_file`.  we use `source` (or `.`)
to maintain state and avoid variable expansion issues with `eval`,
so internally we do `source .tmp.file` where `.tmp.file` is created
with the commands after `# ⬇EXECUTE⬇`.  we use `inotify` (Linux-only)
to notify `fthis` of when `some_file` changes, in `wthis` (Watch THIS).
`fthis` will automatically call `wthis` as needed.

## future

a longterm (but low priority) goal of mine is to create a file editor
that makes it easy to have a shell open and run commands like this.
