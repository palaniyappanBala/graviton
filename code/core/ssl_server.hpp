/**
 * @file
 *
 * @author  Sina Hatef Matbue ( _null_ ) <sinahatef.cpp@gmail.com>
 *
 * @section License
 *
 * This file is part of GraVitoN.
 *
 * Graviton is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Graviton is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Graviton.  If not, see http://www.gnu.org/licenses/.
 *
 * @brief GraVitoN::Core::SSL_Server
 */

#ifndef GRAVITON_SSL_SERVER_H
#define GRAVITON_SSL_SERVER_H

#include <graviton.hpp>
#include <core/ssl_socket.hpp>
#include <core/ssl_client.hpp>
#include <core/thread.hpp>
#include <list>

namespace GraVitoN
{
    namespace Core
    {

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        /// @todo inherit SSL_Server from TCP_Server
        class SSL_Server
        {
        protected:
            unsigned int port;
            bool multi_thread;

            string bind_ip;
            
            string cert_file;
            string key_file;
            // SSL_CTX* ctx;

            struct sockaddr_in sa_serv;

            Socket::Handle listen_sock;

            /// Server socket thread
            class SERVER_INTERNAL_THREAD : public GraVitoN::Core::Thread
            {
            private:
                SSL_Server &server_handle;
                SSL_Client &sock;

            protected:
                bool main()
                    {
                        try
                        {
//                if( !server_handle.initializeConnection(sock) )
//                {
//                    sock.close();
//                    return false;
//                }

                            bool res = server_handle.response(sock);

                            if( !server_handle.finalizeConnection(sock) )
                            {
                                sock.close();
                                return false;
                            }

                            return res;
                        }
                        catch(...)
                        {
                            Logger::logItLn("[SSL Server] Client Thread - Exception");
                            sock.close();
                        }

                        return false;
                    }

            public:
                SERVER_INTERNAL_THREAD(SSL_Server &_this_server, SSL_Client &client_sock)
                    :server_handle(_this_server),
                     sock(client_sock)
                    {
                        // server_handle = _this_server;
                        // sock = client_sock;
                    }

                virtual ~SERVER_INTERNAL_THREAD()throw()
                    {
                        sock.close();
                    }
            };
            list<SERVER_INTERNAL_THREAD*> internal_threads;

        public:
            virtual bool initializeConnection(GraVitoN::Core::SSL_Client &client_sock);
            virtual bool response(GraVitoN::Core::SSL_Client &client_sock) = 0;
            virtual bool finalizeConnection(GraVitoN::Core::SSL_Client &client_sock);

        public:
            SSL_Server(const string &bind_ip_,
                       const unsigned int local_port,
                       const string &_cert_file,
                       const string &_key_file,
                       const bool enable_multi_thread = true)
                {
                    SSL_Socket::getInstance();

                    cert_file = _cert_file;
                    key_file = _key_file;
                    multi_thread = enable_multi_thread;
                    port = local_port;

                    bind_ip = bind_ip_;
                    
                    listen_sock = socket (AF_INET, SOCK_STREAM, 0);
                }

            virtual bool run();
            virtual bool bind();
            virtual bool listen();
            virtual bool accept(Socket::Handle &handle, Socket::Address &addr);
            virtual bool close();
        };

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        bool SSL_Server::run()
        {
            if( bind() )
            {
                if( listen() )
                {
                    close();
                    return true;
                }
                else
                {
                    return false;
                }
            }
            return true;
        }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        bool SSL_Server::bind()
        {
            try
            {
                memset (&sa_serv, '\0', sizeof(sa_serv));
                sa_serv.sin_family      = AF_INET;
                sa_serv.sin_addr.s_addr = inet_addr(bind_ip.c_str());
                sa_serv.sin_port        = htons (port);          /* Server Port number */

                int err = ::bind(listen_sock, (struct sockaddr*) &sa_serv, sizeof (sa_serv));
                if( Socket::socketError(err) )
                {
                    Core::Logger::logItLn("[TCP SERVER] bind failed");
                    return false;
                }
            }
            catch(...)
            {
                return false;
            }
            return true;
        }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        bool SSL_Server::accept(Socket::Handle &handle, Socket::Address &addr)
        {
            try
            {
                Socket::Size client_len = sizeof(addr);
                handle = ::accept (listen_sock, (struct sockaddr*) &addr, &client_len);
                if( Socket::invalidSocket(handle) )
                {
                    Logger::logItLn("[SSL Server] accept failed");
                    return false;
                }
            }
            catch(...)
            {
                Logger::logItLn("[SSL Server] accept failed - Exception");
                return false;
            }
            return true;
        }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        bool SSL_Server::close()
        {
            try
            {
                if( !internal_threads.empty() )
                {
                    for(list<SERVER_INTERNAL_THREAD*>::iterator p_trd = internal_threads.begin(); p_trd != internal_threads.end(); ++p_trd)
                        (*p_trd)->stop();
                    internal_threads.clear();
                }
                if( !Socket::invalidSocket( listen_sock ) )
                {
#if defined(INFO_OS_WINDOWS)
                    ::closesocket(listen_sock);
#else
                    ::close(listen_sock);
#endif
                }
            }
            catch(...)
            {
                Logger::logItLn("[TCP Server] Close Failed - Exception");
                return false;
            }
            return true;
        }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        bool SSL_Server::listen()
        {
            try
            {
                /* Receive a TCP connection. */
                int err = ::listen (listen_sock, 5);
                if( Socket::socketError(err) )
                {
                    Core::Logger::logItLn("[SSL SERVER] listen failed");
                    return false;
                }

                if( multi_thread )
                {
                    Core::Logger::logItLn("[SSL Server] Listening (MultiThread)...");

                    // list<TCP_Socket> lsock;
                    list<SSL_Client*> client_sock;

                    while( !Socket::invalidSocket(listen_sock) )
                    {
                        try
                        {
                            Socket::Handle clis;
                            Socket::Address addr;

                            if( !accept(clis, addr) )
                                continue;

                            // SSL_Client sslcli(clis, addr);

                            client_sock.push_back( new SSL_Client(clis, addr) );
                            initializeConnection(*client_sock.back());
                            /// Initialize and run response thread.
                            internal_threads.push_back( new SERVER_INTERNAL_THREAD((SSL_Server&)*this, *client_sock.back() ));
                            // cout << "Running..." << endl;
                            internal_threads.back()->run();
                        }
                        catch(...)
                        {
                            Logger::logItLn("[SSL Server] Listen Loop - Exception");
                        }
                    }
                }
                else
                {
                    Core::Logger::logItLn("[SSL Server] Listening (Single Thread)...");
                    while( !Socket::invalidSocket(listen_sock) )
                    {
                        Core::sleep(1);

                        Socket::Handle clis;
                        Socket::Address addr;

                        if( !accept(clis, addr) )
                            continue;

                        SSL_Client cln(clis, addr);

                        try
                        {
                            if( this->initializeConnection( cln ) )
                                if( this->response( cln ) )
                                    this->finalizeConnection( cln );
                        }
                        catch(...)
                        {
                            Logger::logItLn("[SSL Server] Listen Loop - Exception");
                        }
                    }
                }
            }
            catch(...)
            {
                Logger::logItLn("[SSL Server] Listen Loop - Exception");
                close();
                return false;
            }

            return true;
        }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        bool SSL_Server::initializeConnection(GraVitoN::Core::SSL_Client &client_sock)
        {
            client_sock.loadPemFromFile(cert_file, key_file);
            return client_sock.handshake_accept();
        }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
        bool SSL_Server::finalizeConnection(Core::SSL_Client &client_sock)
        {
            // client_sock.close();
            return true;
        }

    }
}
#endif
