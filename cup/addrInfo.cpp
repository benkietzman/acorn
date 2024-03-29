// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : addrInfo.cpp
// author     : Ben Kietzman
// begin      : 2018-12-20
// copyright  : kietzman.org
// email      : ben@kietzman.org
///////////////////////////////////////////

/**************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
**************************************************************************/

/*! \file addrInfo.cpp
* \brief Acorn Cup - addrInfo
*
* Provides the addrInfo cup for an acorn node.
*/
// {{{ includes
#include <cerrno>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
#include <Central>
#include <Json>
#include <SignalHandling>
using namespace common;
// }}}
// {{{ defines
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl
// }}}
// {{{ global variables
bool gbShutdown = false; //!< Global shutdown variable.
string gstrApplication = "Acorn"; //!< Global application name.
string gstrData = "/data/acorn"; //!< Global data directory.
string gstrName; //!< Global acorn or gateway name.
Central *gpCentral = NULL; //!< Contains the Central class.
// }}}
// {{{ prototypes
/*! \fn void initialize(string strPrefix, int argc, char *argv[], string &strError)
* \brief Monitors the health of the running process.
* \param strPrefix Contains the function prefix.
* \param argc Contains the number of command-line arguments.
* \param argv Contains the command-line arguments.
* \return Returns a boolean true/false value.
*/
bool initialize(string strPrefix, int argc, char *argv[], string &strError);
/*! \fn void sighandle(const int nSignal)
* \brief Establishes signal handling for the application.
* \param nSignal Contains the caught signal.
*/
void sighandle(const int nSignal);
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  string strError, strPrefix = "main()";
  stringstream ssMessage;
 
  if (initialize(strPrefix, argc, argv, strError))
  {
    if (!gstrData.empty())
    {
      char szBuffer[65536];
      int nReturn;
      pollfd *fds;
      size_t unIndex, unPosition;
      string strBuffer[2], strJson;
      while (!gbShutdown)
      {
        unIndex = ((!strBuffer[1].empty())?2:1);
        fds = new pollfd[unIndex];
        unIndex = 0;
        fds[unIndex].fd = 0;
        fds[unIndex].events = POLLIN;
        unIndex++;
        if (!strBuffer[1].empty())
        {
          fds[unIndex].fd = 1;
          fds[unIndex].events = POLLOUT;
          unIndex++;
        }
        if ((nReturn = poll(fds, unIndex, 250)) > 0)
        {
          for (size_t i = 0; i < unIndex; i++)
          {
            // {{{ read from cap
            if (fds[i].fd == 0)
            {
              if (fds[i].revents & POLLIN)
              {
                if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                {
                  strBuffer[0].append(szBuffer, nReturn);
                  while ((unPosition = strBuffer[0].find("\n")) != string::npos)
                  {
                    bool bProcessed = false;
                    Json *ptJson = new Json(strBuffer[0].substr(0, unPosition));
                    strBuffer[0].erase(0, (unPosition + 1));
                    strError.clear();
                    if (ptJson->m.find("Request") != ptJson->m.end())
                    {
                      if (ptJson->m["Request"]->m.find("Server") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["Server"]->v.empty())
                      {
                        addrinfo hints, *result;
                        memset(&hints, 0, sizeof(addrinfo));
                        hints.ai_family = AF_UNSPEC;
                        hints.ai_socktype = SOCK_STREAM;
                        if ((nReturn = getaddrinfo(ptJson->m["Request"]->m["Server"]->v.c_str(), NULL, &hints, &result)) == 0)
                        {
                          addrinfo *rp;
                          bProcessed = true;
                          ptJson->m["Response"] = new Json;
                          for (rp = result; rp != NULL; rp = rp->ai_next)
                          {
                            char szIP[INET6_ADDRSTRLEN];
                            string strFamily;
                            void *ptr = NULL;
                            switch (rp->ai_family)
                            {
                              case AF_INET  : strFamily = "IPv4"; ptr = &((struct sockaddr_in *)rp->ai_addr)->sin_addr; break;
                              case AF_INET6 : strFamily = "IPv6"; ptr = &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr; break;
                            }
                            if (ptr != NULL)
                            {
                              inet_ntop(rp->ai_family, ptr, szIP, sizeof(szIP));
                            }
                            if (ptJson->m["Response"]->m.find(strFamily) == ptJson->m["Response"]->m.end())
                            {
                              ptJson->m["Response"]->m[strFamily] = new Json;
                            }
                            ptJson->m["Response"]->m[strFamily]->push_back(szIP);
                          }
                          freeaddrinfo(result);
                        }
                        else
                        {
                          strError = gai_strerror(nReturn);
                        }
                      }
                      else
                      {
                        strError = "Please provide the Server within the Request.";
                      }
                    }
                    else
                    {
                      strError = "Please provide the Request.";
                    }
                    ptJson->insert("Status", ((bProcessed)?"okay":"error"));
                    if (!strError.empty())
                    {
                      ptJson->insert("Error", strError);
                    }
                    strBuffer[1].append(ptJson->json(strJson)+"\n");
                    delete ptJson;
                  }
                }
                else
                {
                  gbShutdown = true;
                  ssMessage.str("");
                  ssMessage << strPrefix << "->read(" << errno << ") error:  " << strerror(errno);
                  gpCentral->log(ssMessage.str(), strError);
                }
              }
            }
            // }}}
            // {{{ write to cap
            if (fds[i].fd == 1)
            {
              if (fds[i].revents & POLLOUT)
              {
                if ((nReturn = write(fds[i].fd, strBuffer[1].c_str(), strBuffer[1].size())) > 0)
                {
                  strBuffer[1].erase(0, nReturn);
                }
                else
                {
                  gbShutdown = true;
                  ssMessage.str("");
                  ssMessage << strPrefix << "->write(" << errno << ") error:  " << strerror(errno);
                  gpCentral->log(ssMessage.str(), strError);
                }
              }
            }
            // }}}
          }
        }
        else if (nReturn < 0)
        {
          gbShutdown = true;
          ssMessage.str("");
          ssMessage << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno);
          gpCentral->log(ssMessage.str(), strError);
        }
        delete[] fds;
      }
      gbShutdown = true;
    }
    else
    {
      mUSAGE(argv[0]);
    }
  }
  else
  {
    ssMessage.str("");
    ssMessage << strPrefix << "->initialize() error:  " << strError;
    gpCentral->log(ssMessage.str(), strError);
  }

  return 0;
}
// }}}
// {{{ initialize()
bool initialize(string strPrefix, int argc, char *argv[], string &strError)
{
  bool bResult = false;
  
  gpCentral = new Central(strError);
  if (strError.empty())
  {
    bResult = true;
    // {{{ set signal handling
    sethandles(sighandle);
    signal(SIGBUS, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGCONT, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGSEGV, SIG_IGN);
    signal(SIGWINCH, SIG_IGN);
    // }}} 
    // {{{ command line arguments
    for (int i = 1; i < argc; i++)
    {   
      string strArg = argv[i];
      if (strArg == "-d" || (strArg.size() > 7 && strArg.substr(0, 7) == "--data="))
      {
        if (strArg == "-d" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrData = argv[++i];
        }
        else
        {
          gstrData = strArg.substr(7, strArg.size() - 7);
        }
        gpCentral->manip()->purgeChar(gstrData, gstrData, "'");
        gpCentral->manip()->purgeChar(gstrData, gstrData, "\"");
      }
      else if (strArg == "-h" || strArg == "--help")
      {
        bResult = false;
        mUSAGE(argv[0]);
      }
      else
      {
        bResult = false;
        strError = (string)"Illegal option \"" + strArg + (string)"\".";
        mUSAGE(argv[0]);
      }
    }
    // }}}
    gpCentral->setApplication(gstrApplication);
    gpCentral->setLog(gstrData, "addrInfo_", "monthly", true, true);
    gpCentral->setRoom("#system");
  }

  return bResult;
}
// }}}
// {{{ sighandle()
void sighandle(const int nSignal)
{
  string strError, strSignal;
  stringstream ssMessage;

  sethandles(sigdummy);
  gbShutdown = true;
  if (nSignal != SIGINT && nSignal != SIGTERM)
  {
    ssMessage.str("");
    ssMessage << "sighandle(" << nSignal << ") error:  Caught a " << sigstring(strSignal, nSignal) << " signal.";
    gpCentral->log(ssMessage.str(), strError);
  }
  exit(1);
}
// }}}
