# Whisp - Private group chat for linux (and WSL)
A lightweight, self-hosted chat server that uses Tor onion routing for anonymous communication.

![alt text](Whisp.gif)

## Architecture:
Client -> Tor SOCKS5 -> Hidden Service -> Whisp Server -> Hidden Service -> Tor SOCKS5 -> Client

## Features
- **Lightweight**: Written in C to keep things simple - feel free to fork
- **Anonymous**: All traffic routed through Tor - nobody can see your IP
- **Ephemeral**: Messages exist only during active chat sessions - no persistent storage
- **Small groups**: Designed for 2-4 people per session - can *probably* handle many more
- **Private**: Unless you leak the .onion address 



## Server Setup (Host)
1. **Install Tor and Libsodium:**
    ```bash
    sudo apt update
    sudo apt install tor libsodium-dev
2. **Run Tor:**
    ```bash
    sudo systemctl start tor
3. **Configure Tor hidden service** - Follow this guide: https://community.torproject.org/onion-services/setup/
   
4. **Get your onion address** from `/var/lib/tor/hidden_service/hostname`

5. **Compile the server:**
   ```bash
   gcc -pthread server.c utils.c -lsodium -o whisp-server
6. **Run the server:**
   ```bash
    ./whisp-server
7. **Share your .onion address with friends** - DO NOT LEAK IT
## Client Setup (Connect)

1. **Install Tor:**
    ```bash
    sudo apt update
    sudo apt install tor
2. **Run Tor:**
    ```bash
    sudo tor
2. **Create .env with the onion address** 
   ```bash
   echo "WHISP_ONION=your_friends_address.onion" > .env
3. **Compile the server:**
   ```bash
   gcc client.c utils.c -o client
4. Make run script executable:
   ```bash
   chmod +x runClient.sh
5. **Connect:**
   ```bash
    ./runClient.sh