# Whisp
## Private Ephemeral Chat App
Client -> Tor -> VPS -> Whisp (dumb relay) -> VPS -> Tor -> Client

- Host client creates a session. The server assigns a sessionID. The host also gets an encryption key.
- Host shares sessiodID and key with another client.
- sessionID allows the server to establish connection between clients, but the server never seens the encryption key.
- Encryption key is used to decode messages locally within a session.



- sudo -u debian-tor tor
- ps aux | grep tor



gcc -Wall -Wextra -pedantic -pthread server.c utils.c -lsodium -o whisp-server
gcc -Wall -Wextra -pedantic -o client client.c utils.c
