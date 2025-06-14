# TODO
- ~~Setup Tor, seems easy~~
- Simple client file, build with tor. Should be able to send the file to ANY computer, run it, and see the server response message.
- Diffie-Hellman Key Exchange for client to client messages
- I think it should be a very very simple ui. Keep messages on screen for few seconds, then blow away.




# Network
- Partial sends
- Serialization??? - Maybe no need to do anything special here, only sending text. Keep it simple.
- Encapsulation protocol - Keep it similar to beej's example


- I think relay server is the way to go
- A message can be broadcast or direct
- Broadcasts messages go into global chat. Users can set a global chat filter (using the key of the client who sent the message) to decrypt it. Other users will see nonsense.
- Direct messsages must specify the recipient id. Shows up directy on sub panel for recipient.

