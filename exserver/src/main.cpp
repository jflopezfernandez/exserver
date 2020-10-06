/**
 * exServer - Modern high-performance web server
 * Copyright (C) 2020 Jose Fernando Lopez Fernandez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#include <cstdbool>
#include <climits>
#include <cfloat>
#include <cmath>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <boost/program_options.hpp>

namespace Options = boost::program_options;

namespace Default
{
    const std::string ServerPort = "8080";
} // namespace Default

namespace Configuration
{
    std::string ServerPort { Default::ServerPort };
} // namespace Configuration

int main(int argc, const char *argv[])
{
    Options::options_description server_options("Server Options");
    server_options.add_options()
        ("port", Options::value<std::string>(&Configuration::ServerPort)->default_value(Default::ServerPort), "Server listener port")
        ("threads", Options::value<size_t>()->default_value(1), "Number of threads per process to spawn")
    ;

    Options::options_description debugging_options("Debugging Options");
    debugging_options.add_options()
        ("debug", "Enable debug mode assertions and logging")
    ;

    Options::options_description generic_options("Generic Options");
    generic_options.add_options()
        ("help", "Display this help menu and exit")
        ("version", "Display server version information and exit")
        ("verbose", "Output detailed info during execution")
    ;

    Options::options_description options;
    options
        .add(server_options)
        .add(debugging_options)
        .add(generic_options);

    Options::variables_map vm;
    Options::store(
        Options::command_line_parser(argc, argv)
            .options(options)
            .run(),
        vm
    );
    Options::notify(vm);

    if (vm.count("help")) {
        std::cout << options << std::endl;
        return EXIT_SUCCESS;
    }

    if (vm.count("version")) {
        std::cout << "Version 0.1.0" << std::endl;
        return EXIT_SUCCESS;
    }

    std::clog << "eXServer starting..." << "\n";

    struct addrinfo hints;
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* server_address;
    getaddrinfo(0, Configuration::ServerPort.c_str(), &hints, &server_address);

    int server_socket = socket(server_address->ai_family, server_address->ai_socktype, server_address->ai_protocol);

    if (server_socket == -1) {
        std::cerr << "socket() failed" << "\n";
        return EXIT_FAILURE;
    }

    if (bind(server_socket, server_address->ai_addr, server_address->ai_addrlen)) {
        std::cerr << "bind() failed" << "\n";
        return EXIT_FAILURE;
    }

    freeaddrinfo(server_address);

    std::clog << "Listening on port " << Configuration::ServerPort << "..." << "\n";

    if (listen(server_socket, 10) == -1) {
        std::cerr << "listen() failed" << "\n";
        return EXIT_FAILURE;
    }

    fd_set main_sockets_set;
    FD_ZERO(&main_sockets_set);
    FD_SET(server_socket, &main_sockets_set);
    int max_socket = server_socket;

    std::clog << "Waiting for client connections..." << "\n";

    while (true) {
        fd_set read_sockets_set = main_sockets_set;
        
        if (select(max_socket + 1, &read_sockets_set, NULL, NULL, NULL) == -1) {
            std::cerr << "select() failed" << "\n";
            return EXIT_FAILURE;
        }

        for (int i = 1; i <= max_socket; ++i) {
            if (FD_ISSET(i, &read_sockets_set)) {
                if (i == server_socket) {
                    struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof (client_address);

                    int client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_len);

                    if (client_socket == -1) {
                        std::cerr << "accept() failed" << "\n";
                        return EXIT_FAILURE;
                    }

                    FD_SET(client_socket, &main_sockets_set);

                    if (client_socket > max_socket) {
                        max_socket = client_socket;
                    }

                    char address_buffer[100];
                    getnameinfo((struct sockaddr *) &client_address, client_len, address_buffer, sizeof (address_buffer), 0, 0, NI_NUMERICHOST);
                    std::clog << "New connection from " << address_buffer << "." << "\n";
                } else {
                    char request[4096] = { 0 };
                    ssize_t bytes_received = recv(i, request, 4096, 0);

                    if (bytes_received < 1) {
                        FD_CLR(i, &main_sockets_set);
                        close(i);
                        continue;
                    }

                    std::clog << request << "\n";

                    char* request_type = strtok(request, " ");

                    if (request_type == NULL) {
                        // something went terribly wrong
                        std::cerr << "Failed to get HTTP request method" << "\n";
                        return EXIT_FAILURE;
                    }

                    std::clog << "HTTP Request Type: " << request_type << "\n";

                    char page_buffer[4096] = { 0 };
                    char* page_buffer_iterator = page_buffer;

                    FILE* page = fopen("misc/index.html", "r");

                    int c = 0;

                    while ((c = fgetc(page)) != EOF) {
                        *page_buffer_iterator++ = c;
                    }

                    fclose(page);

                    const char* response = 
                        "HTTP/1.1 200 OK\r\n"
                        "Connection: close\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n"
                        "\r\n";/*
                        "<!DOCTYPE html>\n"
                        "<html lang='en-US'>\n"
                        "<head>\n"
                        "    <meta charset='UTF-8'>\n"
                        "    <meta name='viewport' content='width=device-width, initial-scale=1.0, shrink-to-fit=no'>\n"
                        "\n"
                        "    <title>exServer</title>\n"
                        "</head>\n"
                        "<body>\n"
                        "    <p>Hello, world!</p>\n"
                        "</body>\n"
                        "</html>\n";*/
                    
                    ssize_t bytes_sent = send(i, response, strlen(response), 0);
                    send(i, page_buffer, strlen(page_buffer), 0);

                    FD_CLR(i, &main_sockets_set);
                    close(i);
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
