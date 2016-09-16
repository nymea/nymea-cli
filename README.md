# guh-cli
--------------------------------------------

The *guh-cli* (command line interface) is an admin tool written in python to communicate with the [*guh*](https://github.com/guh/guh) JSON-RPC API and test functionality of *guh*.

### Installation 
--------------------------------------------

If you have added the guh repository to your source liste (see [install guh](https://github.com/guh/guh/wiki/Install)) you can run:

    $ sudo apt-get update
    $ sudo apt-get install guh-cli
    
If you want to download the source code you can run:

    $ sudo apt-get source guh-cli

--------------------------------------------

If you want to install it manually you can clone the repository and start the application directly in your folder.
First make sure you have installed *python 2.7*. You can find the installation instruction on the [official python homepage](https://www.python.org/download/releases/2.7/).

Now you can clone the [guh-cli repository](https://github.com/guh/guh-cli) from github:

    $ git clone https://github.com/guh/guh-cli.git
    $ cd guh-cli/    
    $ ./guh-cli

### Usage 
--------------------------------------------

In order to start guh-cli you need to know on which host *guh* is currently running. If guh is running on `localhost`, you can start the application as follows:

    $ guh-cli

If you need help you can run:

    $ man guh-cli
    
or you can run directly:

    $ guh-cli -h
    usage: guh-cli [-h] [-v] [--host HOST] [--port PORT]

    The guh-cli (command line interface) is an admin tool written in python to
    communicate with the guh daemon JSON-RPC API and test functionality of guh.

    optional arguments:
      -h, --help     show this help message and exit
      -v, --version  show program's version number and exit
      --host HOST    the location of the guh daemon (default 127.0.0.1)
      --port PORT    the port of the the guh daemon (default 2222)


Once *guh-cli* has established the connection to guhd, you will see the main menu. In the main menu you can either use the arrow keys to navigate or the item number of the menu list.

In the you are using the `Log monitor` the output will auto scroll down if the marked line is at the end of the log list. If you are navigating in previous logs a terminal flash will inform you about a new log entry. With the `space` key you can jump down to the newest log entry and the auto scroll will be enabled again.


