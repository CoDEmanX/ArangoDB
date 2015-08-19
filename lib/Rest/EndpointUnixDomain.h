////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint via Unix domain socket
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_ENDPOINT_UNIX_DOMAIN_H
#define ARANGODB_REST_ENDPOINT_UNIX_DOMAIN_H 1

#include "Basics/Common.h"

#ifdef TRI_HAVE_LINUX_SOCKETS
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/file.h>

#include "Rest/Endpoint.h"

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                                EndpointUnixDomain
// -----------------------------------------------------------------------------

    class EndpointUnixDomain : public Endpoint {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an endpoint
////////////////////////////////////////////////////////////////////////////////

        EndpointUnixDomain (const EndpointType,
                            const std::string&,
                            int,
                            const std::string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint
////////////////////////////////////////////////////////////////////////////////

        ~EndpointUnixDomain ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief connect the endpoint
////////////////////////////////////////////////////////////////////////////////

        TRI_socket_t connect (double, double) override;

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect the endpoint
////////////////////////////////////////////////////////////////////////////////

        void disconnect () override;

////////////////////////////////////////////////////////////////////////////////
/// @brief init an incoming connection
////////////////////////////////////////////////////////////////////////////////

        bool initIncoming (TRI_socket_t) override;

////////////////////////////////////////////////////////////////////////////////
/// @brief get endpoint domain
////////////////////////////////////////////////////////////////////////////////

        int getDomain () const override {
          return AF_UNIX;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get port
////////////////////////////////////////////////////////////////////////////////

        int getPort () const override {
          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get host name
////////////////////////////////////////////////////////////////////////////////

        std::string getHost () const override {
          return "localhost";
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get host string for HTTP requests
////////////////////////////////////////////////////////////////////////////////

        std::string getHostString () const override {
          return "localhost";
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief socket file
////////////////////////////////////////////////////////////////////////////////

        std::string _path;

    };

  }
}

#endif

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
