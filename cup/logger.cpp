// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : logger.cpp
// author     : Ben Kietzman
// begin      : 2019-01-23
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

/*! \file logger.cpp
* \brief Acorn Cup - logger
*
* Provides the logger cup for an acorn node.
*/
// {{{ includes
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
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -s SERVER, --server=SERVER" << endl << "     Provides the Logger server." << endl << endl
// }}}
// {{{ structs
struct conn
{
  addrinfo *result;
  bool bFirst;
  int fdConnecting;
  int fdSocket;
  long lArg;
  sockaddr *addr;
  socklen_t addrlen;
  string strBuffer[2];
  Json *ptJson;
};
// }}}
// {{{ global variables
bool gbShutdown = false; //!< Global shutdown variable.
string gstrApplication = "Acorn"; //!< Global application name.
string gstrData = "/data/acorn"; //!< Global data directory.
string gstrName; //!< Global acorn or gateway name.
string gstrServer = "localhost"; //!< Global Logger server.
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
      addrinfo hints, *result;
      bool bClose;
      char szBuffer[65536];
      int fdConnecting = -1, fdSocket = -1, nReturn;
      list<conn *> conns, removeConns, subConns;
      list<list<conn *>::iterator> removals;
      long lArg;
      pollfd *fds;
      size_t unIndex, unPosition;
      sockaddr *addr;
      socklen_t addrlen;
      string strBuffer[2], strJson, strLoggerBuffer[2];
      Json *ptJson;
      while (!gbShutdown)
      {
        bClose = false;
        // {{{ prep sockets
        unIndex = ((!strBuffer[1].empty())?2:1);
        // {{{ logger
        if (fdSocket != -1)
        {
          unIndex++;
        }
        else if (fdConnecting != -1)
        {
          if (connect(fdConnecting, addr, addrlen) == 0)
          {
            unIndex++;
            fdSocket = fdConnecting;
            fdConnecting = -1;
            if (lArg >= 0)
            {
              fcntl(fdSocket, F_SETFL, lArg);
            }
            ssMessage.str("");
            ssMessage << strPrefix << "->connect():  Connected to Logger.";
            gpCentral->log(ssMessage.str(), strError);
          }
          else if (errno != EALREADY && errno != EINPROGRESS)
          {
            close(fdConnecting);
            fdConnecting = -1;
            freeaddrinfo(result);
          }
        }
        else
        {
          bool bConnected[2] = {false, false};
          memset(&hints, 0, sizeof(addrinfo));
          hints.ai_family = AF_UNSPEC;
          hints.ai_socktype = SOCK_STREAM;
          if ((nReturn = getaddrinfo(gstrServer.c_str(), "5648", &hints, &result)) == 0)
          {
            addrinfo *rp;
            for (rp = result; !bConnected[1] && rp != NULL; rp = rp->ai_next)
            {
              bConnected[0] = false;
              if ((fdConnecting = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
              {
                long lSubArg;
                addr = rp->ai_addr;
                addrlen = rp->ai_addrlen;
                if ((lArg = lSubArg = fcntl(fdConnecting, F_GETFL, NULL)) >= 0)
                {
                  lSubArg |= O_NONBLOCK;
                  fcntl(fdConnecting, F_SETFL, lSubArg);
                }
                if (connect(fdConnecting, addr, addrlen) == 0)
                {
                  unIndex++;
                  fdSocket = fdConnecting;
                  fdConnecting = -1;
                  if (lArg >= 0)
                  {
                    fcntl(fdSocket, F_SETFL, lArg);
                  }
                  ssMessage.str("");
                  ssMessage << strPrefix << "->connect():  Connected to Logger.";
                  gpCentral->log(ssMessage.str(), strError);
                }
                else if (errno != EALREADY && errno != EINPROGRESS)
                {
                  close(fdConnecting);
                  fdConnecting = -1;
                  freeaddrinfo(result);
                }
              }
              else
              {
                freeaddrinfo(result);
              }
            }
          }
        }
        // }}}
        // {{{ conns
        for (auto &i : conns)
        {
          if (i->fdSocket != -1)
          {
            unIndex++;
          }
          else if (i->fdConnecting != -1)
          {
            if (connect(i->fdConnecting, i->addr, i->addrlen) == 0)
            {
              unIndex++;
              i->fdSocket = i->fdConnecting;
              i->fdConnecting = -1;
              if (i->lArg >= 0)
              {
                fcntl(i->fdSocket, F_SETFL, i->lArg);
              }
            }
            else if (errno != EALREADY && errno != EINPROGRESS)
            {
              close(i->fdConnecting);
              i->fdConnecting = -1;
              freeaddrinfo(i->result);
            }
          }
          else
          {
            bool bConnected[2] = {false, false};
            memset(&hints, 0, sizeof(addrinfo));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            if ((nReturn = getaddrinfo(gstrServer.c_str(), "5646", &hints, &(i->result))) == 0)
            {
              addrinfo *rp;
              for (rp = i->result; !bConnected[1] && rp != NULL; rp = rp->ai_next)
              {
                bConnected[0] = false;
                if ((i->fdConnecting = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
                {
                  long lArg;
                  i->addr = rp->ai_addr;
                  i->addrlen = rp->ai_addrlen;
                  if ((i->lArg = lArg = fcntl(i->fdConnecting, F_GETFL, NULL)) >= 0)
                  {
                    lArg |= O_NONBLOCK;
                    fcntl(i->fdConnecting, F_SETFL, lArg);
                  }
                  if (connect(i->fdConnecting, i->addr, i->addrlen) == 0)
                  {
                    unIndex++;
                    i->fdSocket = i->fdConnecting;
                    i->fdConnecting = -1;
                    if (i->lArg >= 0)
                    {
                      fcntl(fdSocket, F_SETFL, i->lArg);
                    }
                  }
                  else if (errno != EALREADY && errno != EINPROGRESS)
                  {
                    close(i->fdConnecting);
                    i->fdConnecting = -1;
                    freeaddrinfo(i->result);
                  }
                }
                else
                {
                  freeaddrinfo(i->result);
                }
              }
            }
          }
        }
        // }}}
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
        if (fdSocket != -1)
        {
          fds[unIndex].fd = fdSocket;
          fds[unIndex].events = POLLIN;
          if (!strLoggerBuffer[1].empty())
          {
            fds[unIndex].events |= POLLOUT;
          }
          unIndex++;
        }
        for (auto &i : conns)
        {
          if (i->fdSocket != -1)
          {
            fds[unIndex].fd = i->fdSocket;
            fds[unIndex].events = POLLIN;
            if (!i->strBuffer[1].empty())
            {
              fds[unIndex].events |= POLLOUT;
            }
            unIndex++;
          }
        }
        // }}}
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
                    Json *ptJson = new Json(strBuffer[0].substr(0, unPosition));
                    strBuffer[0].erase(0, (unPosition + 1));
                    if (ptJson->m.find("Function") != ptJson->m.end() && !ptJson->m["Function"]->v.empty())
                    {
                      if (ptJson->m["Function"]->v == "log" || ptJson->m["Function"]->v == "message")
                      {
                        strLoggerBuffer[1].append(ptJson->json(strJson)+"\n");
                      }
                      else if (ptJson->m["Function"]->v == "search")
                      {
                        conn *ptConn = new conn;
                        ptConn->bFirst = true;
                        ptConn->fdConnecting = -1;
                        ptConn->fdSocket = -1;
                        ptConn->strBuffer[1].append(ptJson->json(strJson)+"\n");
                        ptConn->ptJson = new Json(ptJson);
                        conns.push_back(ptConn);
                      }
                      else
                      {
                        ptJson->insert("Status", "error");
                        ptJson->insert("Error", "Please provide a valid Function:  log, message, search.");
                        strBuffer[1].append(ptJson->json(strJson)+"\n");
                      }
                    }
                    else
                    {
                      ptJson->insert("Status", "error");
                      ptJson->insert("Error", "Please provide the Function.");
                      strBuffer[1].append(ptJson->json(strJson)+"\n");
                    }
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
            // {{{ logger
            if (fdSocket != -1 && fds[i].fd == fdSocket)
            {
              // {{{ read from logger
              if (fds[i].revents & POLLIN)
              {
                if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                {
                  strLoggerBuffer[0].append(szBuffer, nReturn);
                  while ((unPosition = strLoggerBuffer[0].find("\n")) != string::npos)
                  {
                    strBuffer[1].append(strLoggerBuffer[0].substr(0, (unPosition + 1)));
                    strLoggerBuffer[0].erase(0, (unPosition + 1));
                  }
                }
                else
                {
                  bClose = true;
                  if (nReturn < 0)
                  {
                    ssMessage.str("");
                    ssMessage << strPrefix << "->read(" << errno << ") error [" << fdSocket << "]:  " << strerror(errno);
                    gpCentral->log(ssMessage.str(), strError);
                  }
                }
              }
              // }}}
              // {{{ write to logger
              if (fds[i].revents & POLLOUT)
              {
                if ((nReturn = write(fds[i].fd, strLoggerBuffer[1].c_str(), strLoggerBuffer[1].size())) > 0)
                {
                  strLoggerBuffer[1].erase(0, nReturn);
                }
                else
                {
                  bClose = true;
                  if (nReturn < 0)
                  {
                    ssMessage.str("");
                    ssMessage << strPrefix << "->write(" << errno << ") error [" << fdSocket << "]:  " << strerror(errno);
                    gpCentral->log(ssMessage.str(), strError);
                  }
                }
              }
              // }}}
            }
            // }}}
            // {{{ conns
            for (auto j = conns.begin(); j != conns.end(); j++)
            {
              if ((*j)->fdSocket != -1 && fds[i].fd == (*j)->fdSocket)
              {
                // {{{ read from conn
                if (fds[i].revents & POLLIN)
                {
                  if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                  {
                    (*j)->strBuffer[0].append(szBuffer, nReturn);
                    while ((unPosition = (*j)->strBuffer[0].find("\n")) != string::npos)
                    {
                      ptJson = new Json((*j)->strBuffer[0].substr(0, unPosition));
                      (*j)->strBuffer[0].erase(0, (unPosition + 1));
                      if ((*j)->bFirst)
                      {
                        (*j)->bFirst = false;
                        (*j)->ptJson->merge(ptJson, true, false);
                        if ((*j)->ptJson->m.find("Response") != (*j)->ptJson->m.end())
                        {
                          delete (*j)->ptJson->m["Response"];
                        }
                        (*j)->ptJson->m["Response"] = new Json;
                      }
                      else
                      {
                        (*j)->ptJson->m["Response"]->push_back(ptJson);
                      }
                      delete ptJson;
                    }
                  }
                  else
                  {
                    removals.push_back(j);
                    if (nReturn < 0)
                    {
                      ssMessage.str("");
                      ssMessage << strPrefix << "->read(" << errno << ") error [" << (*j)->fdSocket << "]:  " << strerror(errno);
                      gpCentral->log(ssMessage.str(), strError);
                    }
                  }
                }
                // }}}
                // {{{ write to conn
                if (fds[i].revents & POLLOUT)
                {
                  if ((nReturn = write(fds[i].fd, (*j)->strBuffer[1].c_str(), (*j)->strBuffer[1].size())) > 0)
                  {
                    (*j)->strBuffer[1].erase(0, nReturn);
                  }
                  else
                  {
                    removals.push_back(j);
                    if (nReturn < 0)
                    {
                      ssMessage.str("");
                      ssMessage << strPrefix << "->write(" << errno << ") error [" << (*j)->fdSocket << "]:  " << strerror(errno);
                      gpCentral->log(ssMessage.str(), strError);
                    }
                  }
                }
                // }}}
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
        // {{{ logger
        if (bClose)
        {
          ssMessage.str("");
          if (close(fdSocket) == 0)
          {
            ssMessage << strPrefix << "->close(" << errno << ") [" << fdSocket << "]:  Disconnected from Logger.";
          }
          else
          {
            ssMessage << strPrefix << "->close(" << errno << ") error [" << fdSocket << "]:  " << strerror(errno);
          }
          gpCentral->log(ssMessage.str(), strError);
          fdSocket = -1;
        }
        // }}}
        // {{{ conns
        if (!removals.empty())
        {
          for (auto i = conns.begin(); i != conns.end(); i++)
          {
            bool bFound = false;
            for (auto j = removals.begin(); !bFound && j != removals.end(); j++)
            {
              if (i == (*j))
              {
                bFound = true;
              }
            }
            if (bFound)
            {
              removeConns.push_back(*i);
            }
            else
            {
              subConns.push_back(*i);
            }
          }
          removals.clear();
          while (!removeConns.empty())
          {
            if (removeConns.front()->fdSocket != -1)
            {
              close(removeConns.front()->fdSocket);
            }
            else if (removeConns.front()->fdConnecting != -1)
            {
              close(removeConns.front()->fdConnecting);
            }
            if (removeConns.front()->ptJson != NULL)
            {
              strBuffer[1].append(removeConns.front()->ptJson->json(strJson)+"\n");
              delete removeConns.front()->ptJson;
            }
            delete removeConns.front();
            removeConns.pop_back();
          }
          conns = subConns;
          subConns.clear();
        }
        // }}}
      }
      gbShutdown = true;
      // {{{ logger
      if (fdSocket != -1)
      {
        ssMessage.str("");
        if (close(fdSocket) == 0)
        {
          ssMessage << strPrefix << "->close(" << errno << ") [" << fdSocket << "]:  Disconnected from Logger.";
        }
        else
        {
          ssMessage << strPrefix << "->close(" << errno << ") error [" << fdSocket << "]:  " << strerror(errno);
        }
        gpCentral->log(ssMessage.str(), strError);
      }
      else if (fdConnecting != -1)
      {
        ssMessage.str("");
        if (close(fdConnecting) == 0)
        {
          ssMessage << strPrefix << "->close(" << errno << ") [" << fdConnecting << "]:  Disconnected from Logger.";
        }
        else
        {
          ssMessage << strPrefix << "->close(" << errno << ") error [" << fdConnecting << "]:  " << strerror(errno);
        }
        gpCentral->log(ssMessage.str(), strError);
      }
      // }}}
      // {{{ conns
      while (!conns.empty())
      {
        if (conns.front()->fdSocket != -1)
        {
          close(conns.front()->fdSocket);
        }
        else if (conns.front()->fdConnecting != -1)
        {
          close(conns.front()->fdConnecting);
        }
        if (conns.front()->ptJson != NULL)
        {
          delete conns.front()->ptJson;
        }
        delete conns.front();
        conns.pop_back();
      }
      // }}}
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
      else if (strArg == "-s" || (strArg.size() > 9 && strArg.substr(0, 9) == "--server="))
      {
        if (strArg == "-s" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrServer = argv[++i];
        }
        else
        {
          gstrServer = strArg.substr(9, strArg.size() - 9);
        }
        gpCentral->manip()->purgeChar(gstrServer, gstrServer, "'");
        gpCentral->manip()->purgeChar(gstrServer, gstrServer, "\"");
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
    gpCentral->setLog(gstrData, "logger_", "monthly", true, true);
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
