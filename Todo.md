# TODO
- ~~Setup Tor, seems easy~~
- ~~Simple client file, build with tor. Should be able to send the file to ANY computer, run it, and see the server response message~~
- ~~Session creation. User creates session, gets sessionid, shares it, chats with other user.~~

- ~~Session management~~
- Fix naming conventions and clean up functions

- mutex lock for each sesison
- threadcleanup incase of failure during the chat loop

- Put function declarations in .h

- Partial sends
- Serialization??? - Maybe no need to do anything special here, only sending text. Keep it simple.
- Encapsulation protocol - Keep it similar to beej's example

- Message encryption
- Simple ui. Keep messages on screen for few seconds, then blow away.
- Build system
- Instructions on how to host own server, and YT video




- https://community.torproject.org/onion-services/setup/
- Seems easy. Install tor, add a couple lines to the torrc file