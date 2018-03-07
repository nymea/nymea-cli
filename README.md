# nymea-cli
--------------------------------------------

The *nymea-cli* (command line interface) is an admin tool written in python to communicate with the [*nymea*](https://github.com/guh/nymea) JSON-RPC API and test functionality of *nymea*.

### Installation 
--------------------------------------------

If you have added the nymea repository to your source liste (see [install nymea](https://github.com/guh/nymea/wiki/Install)) you can run:

    $ sudo apt-get update
    $ sudo apt-get install nymea-cli
    
If you want to download the source code you can run:

    $ sudo apt-get source nymea-cli

--------------------------------------------

If you want to install it manually you can clone the repository and start the application directly in your folder.
First make sure you have installed *python 2.7*. You can find the installation instruction on the [official python homepage](https://www.python.org/download/releases/2.7/).

Now you can clone the [nymea-cli repository](https://github.com/guh/nymea-cli) from github:

    $ git clone https://github.com/guh/nymea-cli.git
    $ cd nymea-cli/
    $ ./nymea-cli

### Usage 
--------------------------------------------

In order to start nymea-cli you need to know on which host *nymea* is currently running. If nymea is running on `localhost`, you can start the application as follows:

    $ nymea-cli

If you need help you can run:

    $ man nymea-cli
    
or you can run directly:

    $ nymea-cli -h
    usage: nymea-cli [-h] [-v] [--host HOST] [--port PORT]

    The nymea-cli (command line interface) is an admin tool written in python to
    communicate with the nymea daemon JSON-RPC API and test functionality of nymea.

    optional arguments:
      -h, --help     show this help message and exit
      -v, --version  show program's version number and exit
      --host HOST    the location of the nymea daemon (default 127.0.0.1)
      --port PORT    the port of the the nymea daemon (default 2222)


Once *nymea-cli* has established the connection to nymead, you will see the main menu. In the main menu you can either use the arrow keys to navigate or the item number of the menu list.

In the you are using the `Log monitor` the output will auto scroll down if the marked line is at the end of the log list. If you are navigating in previous logs a terminal flash will inform you about a new log entry. With the `space` key you can jump down to the newest log entry and the auto scroll will be enabled again.
