#include <iostream>
#include <fstream>
#include <SFML/Network.hpp>
#include <sstream>
#include "picosha2.h"

struct Client {
    std::string login;
    std::string passwordHash;
    
    std::string config;
};

#define ClientsFileName "clients.list"

void writeClient(std::ofstream& fout, Client& client) {
    fout << client.login << '\0' << client.passwordHash << '\0' << client.config << '\0';
}

Client readClient(std::ifstream& fin) {
    Client c;
    std::getline(fin, c.login, '\0');
    std::getline(fin, c.passwordHash, '\0');
    std::getline(fin, c.config, '\0');
    return c;
}

void writeVector(std::vector<Client>& clients) {
    std::ofstream fout(ClientsFileName);
    fout << clients.size() << '\0';
    for (int i = 0; i < clients.size(); i++) {
        writeClient(fout, clients[i]);
    }
}

std::vector<Client> readVector() {
    std::ifstream fin(ClientsFileName);
    std::vector<Client> clients;

    std::string size;
    std::getline(fin, size, '\0');

    std::cout << "There should be " << size << " clients in the file\n";
    for (int i = 0; i < std::stoi(size); i++) {
        clients.push_back(readClient(fin));
    }
    std::cout << "Read " << clients.size() << " in total\n";

    return clients;
}

bool userExists(std::vector<Client>& clients, std::string login) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].login == login) return true;
    }
    return false;
}

// Проверять на nullptr!
Client* authorize(std::vector<Client>& clients, std::string login, std::string password) {
    if (!userExists(clients, login)) return nullptr;
    std::string passwordHash;
    picosha2::hash256_hex_string("s4lt_" + password + "_s4lt", passwordHash);

    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].login == login && clients[i].passwordHash == passwordHash)
            return &clients[i];
    }

    return nullptr;
}

Client* authorize(std::vector<Client>& clients, sf::TcpSocket* socket, sf::Packet& packet_in, sf::Packet& packet_out) {
    std::string login, password;
    packet_in >> login >> password;
                                
    Client* c = authorize(clients, login, password);
    if (c == nullptr) {
        packet_out << "auth_failed";
        socket->send(packet_out);
    }
    else {
        packet_out << "success";
    }

    return c;
}

int main()
{
    std::string defaultConfig;
    std::ifstream fin("default_config.ini");
    std::stringstream buffer;
    buffer << fin.rdbuf();
    defaultConfig = buffer.str(); 
    fin.close();

    std::vector<Client> clients;
    fin.open(ClientsFileName);
    // Проверка на существование файла
    if (fin) {
        std::cout << "Clients file exists, reading...\n";
        fin.close();
        clients = readVector();
    }

    std::vector<sf::TcpSocket*> sockets;

    sf::TcpListener listener;
    sf::SocketSelector selector;

    if (listener.listen(8080) != sf::Socket::Done) {
        std::cout << "Can't start on " << 8080 << '\n';
        return -1;
    }
    std::cout << "Port: " << 8080 << '\n';
        
    selector.add(listener);

    while (true) {
        if (selector.wait()) {
            if (selector.isReady(listener)) {
                sf::TcpSocket* socket = new sf::TcpSocket;
                if (listener.accept(*socket) != sf::Socket::Done) {
                    std::cout << "Error while adding new\n";
                    delete socket;
                }
                else {
                    std::cout << "Adding new\n";
                    sockets.push_back(socket);
                    selector.add(*socket);
                }
            }
            else {
                for (int i = 0; i < sockets.size(); i++) {
                    sf::TcpSocket* socket = sockets[i];
                    if (selector.isReady(*socket)) {
                        sf::Packet packet_in, packet_out;
                        sf::Socket::Status status = socket->receive(packet_in);
                        if (status == sf::Socket::Done) {
                            std::string command;
                            packet_in >> command;

                            if (command == "create_user") {
                                std::string login, password;
                                packet_in >> login >> password;
                                if (userExists(clients, login)) {
                                    packet_out << "user_exists";
                                    sockets[i]->send(packet_out);
                                }
                                else {
                                    Client c;
                                    c.login = login;
                                    c.config = defaultConfig;
                                    // Хешируем password и помещаем в c.passwordHash
                                    picosha2::hash256_hex_string("s4lt_" + password + "_s4lt", c.passwordHash);
                                    clients.push_back(c);

                                    writeVector(clients);
                                    
                                    packet_out << "success";
                                    sockets[i]->send(packet_out);
                                }
                            }
                            else if (command == "get_config") {                               
                                Client* c = authorize(clients, sockets[i], packet_in, packet_out);
                                if (c != nullptr) {
                                    packet_out << c->config; 
                                    sockets[i]->send(packet_out);
                                }
                            }
                            else if (command == "send_config") {
                                Client* c = authorize(clients, sockets[i], packet_in, packet_out);
                                if (c != nullptr) {
                                    packet_in >> c->config;
                                    writeVector(clients);
                                    sockets[i]->send(packet_out);
                                }
                            }
                            else if (command == "test_login") {
                                Client* c = authorize(clients, sockets[i], packet_in, packet_out);
                                if (c != nullptr) {
                                    sockets[i]->send(packet_out);
                                }
                            }
                            else {
                                std::cout << "Unknown command: " << command << " \n";
                            }
                        }
                        else if (status == sf::Socket::Disconnected) {
                            selector.remove(*socket);
                            delete socket;
                            sockets.erase(sockets.begin() + i);
                            std::cout << "Socket disconnected\n";
                            break;
                        }
                        else {
                            std::cout << "Packet error\n";
                        }
                    }
                }
            }
        }
    }

}
