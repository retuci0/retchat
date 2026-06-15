cmake_minimum_required(VERSION 3.16)
project(RetchatServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(server
    src/Client.cpp
    src/DiffieHellman.cpp
    src/Packet.cpp
    src/Room.cpp
    
    src/Server.cpp
)

target_include_directories(server PRIVATE 
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(server ${OPENSSL_LIBRARIES} pthread)