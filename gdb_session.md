# Cpp Guild: Tools for debugging: gdb

Tue October 10, 2023 at 14:00-16:00

## Teams

Teams Meeting, join at:

    https://knowit.sharepoint.com/sites/Org-510-internal/SitePages/Competence-Development-Calendar.aspx

## Instructions for participants

The hands-on part with gdb is workable through an ssh connection:

    ssh -i cpp-gdb.pem ubuntu@ec2-16-16-63-61.eu-north-1.compute.amazonaws.com

We will provide you with the pem key pair:

    cpp-gdb.pem

in a zip file:

    login.zip

When you use the key pair in Linux/Mac, please remember to change the file mode:

    chmod 400 cpp-gdb.pem

and use ssh only after that in your terminal.

In Windows, you may use PowerShell for the ssh connection.

When you have logged in, you are instructed to use a specific one of directories:

    user1
    user2
    user3
    ...

Represent your directory by *userX*.

Within *userX*, you can find a git directory where you can find the source code of the exercise:

    jsonizer.cpp

We will advise you further during the session. Happy gdb'ing!

## About this doc

This document is Markdown, or HTML rendering of Markdown. The rendering is done with Linux application _markdown_,
installable with:

    sudo apt install markdown

Then:

    markdown document.md > document.html

## About the session

There are at least 3 common usage scenarios for the gdb debugger:

* run an application in gdb
* attach to a running application
* remote debug.

We shall focus on running the app in gdb. This is the simplest way to introduce the usage of the debugger.

## Building the app

The system under testing in this session is a reasonably small threaded application in a single source file,
*jsonizer.cpp*.

To compile the cpp file, cd into your source directory and use the provided shell script as follows:

    ./compile17.sh jsonizer

To test run the compiled program, use:

    ./jsonizer

__Try this out now__

*To keep things simple, let's stay in this source / binary directory.*

As can be seen, Jsonizer prints some stats to stdout, followed by a set of:

    Result: { JSON contents }

## The backstory

Let us define a role for you participants.

Somebody else has created Jsonizer, and it seemed to work fine.
Except it worked fine only with specific "nice" parametrization. With other kind of parametrization, Jsonizer either
gets stuck or crashes. Looks like testing wasn't done very thoroughly, if at all.

You shall be the maintainer getting things back on track.

## The SUT

The Jsonizer creates semi random JSON files. It has command line options for tweaking the output.

Try:

    ./jsonizer -h

to get command line help.

Then try:

    ./jsonizer -p default
    ./jsonizer -p godbolt
    ./jsonizer -p complex

Things should work fine again, unless you're really unlucky.

Now try:

    for i in {1..100}; do ./jsonizer -p complex; done

At some point, Jsonizer just freezes. Bumped into a bug, it seems.

## Firing up the debugger

Start Jsonizer in gdb like so:

    gdb --args ./jsonizer -p complex

Then type

    run

to run the app. With bad luck, it might get stuck at the first try, but if not, run again until it does.
Use _up arrow_ to access the previous commands.

If for some reason the app keeps on succeeding, we shall need to get into more involved command line parametrization.

_Ctrl-C_ will get you back to gdb command mode.
Typing _q_ or _quit_ will exit the debugging session. Just respond __y__ to the potential _Quit anyway_ prompt.

## Next steps

As typing this markdown file takes up time that doesn't really exist, let's continue the session online.

The topics to get covered are at least:

* backtrace
* changing stack frame
* (changing) threads
* breakpoints
* watchpoints
* better variable printouts
* a trick or two
* hunting the bug
* fixing the bug.
