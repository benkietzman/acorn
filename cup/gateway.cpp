// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : gateway.cpp
// author     : Ben Kietzman
// begin      : 2018-12-18
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

/*! \file gateway.cpp
* \brief Acorn Cup - gateway
*
* Provides the gateway cup for an acorn node.
*/
// {{{ includes
#include <fstream>
#include <iostream>
#include <list>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
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
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -c CONF, --conf=CONF" << endl << "     Sets the configuration file." << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -n NAME, --name=NAME" << endl << "     Provides the gateway name." << endl << endl << " -p PORT, --port=PORT" << endl << "     Used by an external type to provide the listening port number." << endl << endl << " -r ROUTER, --router=ROUTER" << endl << "     Provides the router server." << endl << endl << " -s SERVER, --server=SERVER" << endl << "     Provides the gateway server."
/*! \def UNIX_SOCKET
* \brief Contains the unix socket path.
*/  
#define UNIX_SOCKET "/tmp/acorn_gw"
// }}}
// {{{ structs
struct conn
{
  bool bRegistered;
  int fdSocket;
  string strBuffer[2];
  string strName;
};
// }}}
// {{{ global variables
bool gbShutdown = false; //!< Global shutdown variable.
string gstrApplication = "Acorn"; //!< Global application name.
string gstrData = "/data/acorn"; //!< Global data directory.
string gstrName; //!< Global acorn or gateway name.
string gstrPassword; //<! Contains the password.
string gstrPort; //<! Contains the gateway port.
string gstrRouter = "localhost"; //<! Contains the router server.
string gstrServer; //<! Contains the gateway server.
Central *gpCentral = NULL; //!< Contains the Central class.
// }}}
// {{{ prototypes
/*! \fn bool acornDeregister(string strPrefix, conn *ptConn, string &strError)
* \brief Deregister an acorn.
* \param strPrefix Contains the function prefix.
* \param ptConn Contains the connection.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool acornDeregister(string strPrefix, conn *ptConn, string &strError);
/*! \fn bool acornRegister(string strPrefix, conn *ptConn, const string strName, string &strError)
* \brief Register an acorn.
* \param strPrefix Contains the function prefix.
* \param ptConn Contains the connection.
* \param strName Contains the acorn name.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool acornRegister(string strPrefix, conn *ptConn, const string strName, string &strError);
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
    ssMessage.str("");
    ssMessage << strPrefix << ":  Initialized." << endl;
    gpCentral->log(ssMessage.str(), strError);
    if (!gstrData.empty() && !gstrName.empty() && !gstrPort.empty() && !gstrRouter.empty() && !gstrServer.empty())
    {
      ifstream inCred;
      stringstream ssCred;
      // {{{ load router password
      ssCred << gstrData << "/.cred/router";
      inCred.open(ssCred.str().c_str());
      if (inCred)
      {
        string strLine;
        stringstream ssJson;
        Json *ptCred;
        while (getline(inCred, strLine))
        {
          ssJson << strLine;
        }
        ptCred = new Json(ssJson.str());
        if (ptCred->m.find("Password") != ptCred->m.end() && !ptCred->m["Password"]->v.empty())
        {
          gstrPassword = ptCred->m["Password"]->v;
        }
        else
        {
          ssMessage.str("");
          ssMessage << strPrefix << " error [" << ssCred.str() << ":  Failed to find the Password.";
          gpCentral->log(ssMessage.str(), strError);
        }
        delete ptCred;
      }
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->ifstream(" << errno << ") error [" << ssCred.str() << ":  " << strerror(errno);
        gpCentral->log(ssMessage.str(), strError);
      }
      inCred.close();
      // }}}
      if (!gstrPassword.empty())
      {
        int fdUnix;
        ssMessage.str("");
        ssMessage << strPrefix << ":  Loaded router password." << endl;
        gpCentral->log(ssMessage.str(), strError);
        gpCentral->file()->remove(UNIX_SOCKET);
        if ((fdUnix = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
        {
          sockaddr_un addr;
          ssMessage.str("");
          ssMessage << strPrefix << "->socket():  Created socket." << endl;
          gpCentral->log(ssMessage.str(), strError);
          memset(&addr, 0, sizeof(sockaddr_un));
          addr.sun_family = AF_UNIX;
          strncpy(addr.sun_path, UNIX_SOCKET, sizeof(addr.sun_path) - 1);
          if (bind(fdUnix, (sockaddr *)&addr, sizeof(sockaddr_un)) == 0)
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->bind():  Bound to socket." << endl;
            gpCentral->log(ssMessage.str(), strError);
            if (listen(fdUnix, 5) == 0)
            {
              bool bExit = false, bRegistered = false;
              char szBuffer[65536];
              int nReturn;
              list<conn *> conns;
              list<int> removals;
              pollfd *fds;
              size_t unIndex, unPosition;
              string strBuffer[2], strJson;
              Json *ptJson;
              ssMessage.str("");
              ssMessage << strPrefix << "->listen():  Listening to socket." << endl;
              gpCentral->log(ssMessage.str(), strError);
              // {{{ register gateway
              while (!gbShutdown && !bRegistered)
              {
                Json *ptRequest = new Json, *ptResponse = new Json;
                ptRequest->insert("Acorn", "router");
                ptRequest->insert("Password", gstrPassword);
                ptRequest->insert("Function", "register");
                ptRequest->insert("Server", gstrRouter);
                ptRequest->insert("reqApp", gstrApplication);
                ptRequest->m["Request"] = new Json;
                ptRequest->m["Request"]->insert("Acorn", "gateway");
                ptRequest->m["Request"]->insert("Gateway", gstrName);
                ptRequest->m["Request"]->insert("Server", gstrServer);
                ptRequest->m["Request"]->insert("Port", gstrPort);
                if (gpCentral->acorn()->request(ptRequest, ptResponse, 5, strError))
                {
                  if (ptResponse->m.find("Status") != ptResponse->m.end() && ptResponse->m["Status"]->v == "okay")
                  {
                    bRegistered = true;
                    ssMessage.str("");
                    ssMessage << strPrefix << "->Central::acorn()->request(router) [" << gstrName << "]:  Registered gateway." << endl;
                    gpCentral->log(ssMessage.str(), strError);
                  }
                  else if (ptResponse->m.find("Error") != ptResponse->m.end() && !ptResponse->m["Error"]->v.empty())
                  {
                    ssMessage.str("");
                    ssMessage << strPrefix << "->Central::acorn()->request(router) error [register,gateway]:  " << ptResponse->m["Error"]->v;
                    gpCentral->log(ssMessage.str(), strError);
                  }
                  else
                  {
                    ssMessage.str("");
                    ssMessage << strPrefix << "->Central::acorn()->request(router) error [register,gateway]:  Encountered an unknown error.";
                    gpCentral->log(ssMessage.str(), strError);
                  }
                }
                else
                {
                  ssMessage.str("");
                  ssMessage << strPrefix << "->Central::acorn()->request(router) error [register,gateway]:  " << strError;
                  gpCentral->log(ssMessage.str(), strError);
                }
                delete ptRequest;
                delete ptResponse;
                if (!bRegistered)
                {
                  for (size_t i = 0; !gbShutdown && i < 240; i++)
                  {
                    usleep(250000);
                  }
                }
              }
              // }}}
              while (!gbShutdown && !bExit)
              {
                // {{{ prep sockets
                unIndex = ((!strBuffer[1].empty())?2:1);
                unIndex++;
                unIndex += conns.size();
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
                fds[unIndex].fd = fdUnix;
                fds[unIndex].events = POLLIN;
                unIndex++;
                for (auto &i : conns)
                {
                  fds[unIndex].fd = i->fdSocket;
                  fds[unIndex].events = POLLIN;
                  if (!i->strBuffer[1].empty())
                  {
                    fds[unIndex].events |= POLLOUT;
                  }
                  unIndex++;
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
                            Json *ptRequest = new Json(strBuffer[0].substr(0, unPosition));
                            strBuffer[0].erase(0, (unPosition + 1));
                            if (ptRequest->m.find("Acorn") != ptRequest->m.end() && !ptRequest->m["Acorn"]->v.empty())
                            {
                              list<conn *>::iterator connIter = conns.end();
                              for (auto j = conns.begin(); connIter == conns.end() && j != conns.end(); j++)
                              {
                                if ((*j)->strName == ptRequest->m["Acorn"]->v)
                                {
                                  connIter = j;
                                }
                              }
                              if (connIter != conns.end())
                              {
                                (*connIter)->strBuffer[1].append(ptRequest->json(strJson)+"\n");
                              }
                              else
                              {
                                ptRequest->insert("Status", "error");
                                ptRequest->insert("Error", "Please provide a valid Acorn.");
                                strBuffer[1].append(ptRequest->json(strJson)+"\n");
                              }
                            }
                            else
                            {
                              ptRequest->insert("Status", "error");
                              ptRequest->insert("Error", "Please provide the Acorn.");
                              strBuffer[1].append(ptRequest->json(strJson)+"\n");
                            }
                            if (ptRequest->m.find("Password") != ptRequest->m.end())
                            {
                              delete ptRequest->m["Password"];
                              ptRequest->m.erase("Password");
                            }
                            if (ptRequest->m.find("Request") != ptRequest->m.end())
                            {     
                              delete ptRequest->m["Request"];
                              ptRequest->m.erase("Request");
                            }
                            if (ptRequest->m.find("Response") != ptRequest->m.end())
                            {
                              delete ptRequest->m["Response"];
                              ptRequest->m.erase("Response");
                            }
                            ssMessage.str("");
                            ssMessage << strPrefix << " REQUEST:  " << ptRequest;
                            gpCentral->log(ssMessage.str(), strError);
                            delete ptRequest;
                          }
                        }
                        else
                        {
                          bExit = true;
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
                          bExit = true;
                          ssMessage.str("");
                          ssMessage << strPrefix << "->write(" << errno << ") error:  " << strerror(errno);
                          gpCentral->log(ssMessage.str(), strError);
                        }
                      }
                    }
                    // }}}
                    // {{{ accept incoming connection
                    if (fds[i].fd == fdUnix)
                    {
                      if (fds[i].revents & POLLIN)
                      {
                        int fdData;
                        sockaddr_un cli_addr;
                        socklen_t clilen = sizeof(cli_addr);
                        if ((fdData = accept(fdUnix, (sockaddr *)&cli_addr, &clilen)) >= 0)
                        {
                          conn *ptConn = new conn;
                          ptConn->bRegistered = false;
                          ptConn->fdSocket = fdData;
                          conns.push_back(ptConn);
                        }
                        else
                        {
                          bExit = false;
                          ssMessage.str("");
                          ssMessage << strPrefix << "->accept(" << errno << ") error:  " << strerror(errno);
                          gpCentral->log(ssMessage.str(), strError);
                        }
                      }
                    }
                    // }}}
                    // {{{ downstream clients
                    for (auto &j : conns)
                    {
                      if (fds[i].fd == j->fdSocket)
                      {
                        // {{{ read from client
                        if (fds[i].revents & POLLIN)
                        {
                          if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                          {
                            j->strBuffer[0].append(szBuffer, nReturn);
                            while ((unPosition = j->strBuffer[0].find("\n")) != string::npos)
                            {
                              ptJson = new Json(j->strBuffer[0].substr(0, unPosition));
                              if (ptJson->m.find("_AcornFunction") != ptJson->m.end())
                              {
                                if (ptJson->m["_AcornFunction"]->v == "deregister")
                                {
                                  if (acornDeregister(strPrefix, j, strError))
                                  {
                                    ptJson->insert("Status", "okay");
                                  }
                                  else
                                  {
                                    ptJson->insert("Status", "error");
                                  }
                                }
                                else if (ptJson->m["_AcornFunction"]->v == "register")
                                {
                                  if (ptJson->m.find("_AcornName") != ptJson->m.end() && !ptJson->m["_AcornName"]->v.empty())
                                  {
                                    if (acornRegister(strPrefix, j, ptJson->m["_AcornName"]->v, strError))
                                    {
                                      ptJson->insert("Status", "okay");
                                    }
                                    else
                                    {
                                      ptJson->insert("Status", "error");
                                      ptJson->insert("Error", strError);
                                    }
                                  }
                                  else
                                  {
                                    ptJson->insert("Status", "error");
                                    ptJson->insert("Error", "Please provide the _AcornName.");
                                  }
                                }
                                else
                                {
                                  ptJson->insert("Status", "error");
                                  ptJson->insert("Error", "Please provide a valid _AcornFunction:  deregister, register.");
                                }
                                j->strBuffer[1].append(ptJson->json(strJson)+"\n");
                              }
                              else
                              {
                                strBuffer[1].append(j->strBuffer[0].substr(0, (unPosition + 1)));
                              }
                              j->strBuffer[0].erase(0, (unPosition + 1));
                              delete ptJson;
                            }
                          }
                          else
                          {
                            removals.push_back(j->fdSocket);
                            if (nReturn < 0)
                            {
                              ssMessage.str("");
                              ssMessage << strPrefix << "->read(" << errno << ") error:  " << strerror(errno);
                              gpCentral->log(ssMessage.str(), strError);
                            }
                          }
                        }
                        // }}}
                        // {{{ write to client
                        if (fds[i].revents & POLLOUT)
                        {
                          if ((nReturn = write(fds[i].fd, j->strBuffer[1].c_str(), j->strBuffer[1].size())) > 0)
                          {
                            j->strBuffer[1].erase(0, nReturn);
                          }
                          else
                          {
                            removals.push_back(j->fdSocket);
                            if (nReturn < 0)
                            {
                              ssMessage.str("");
                              ssMessage << strPrefix << "->write(" << errno << ") error:  " << strerror(errno);
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
                  bExit = true;
                  ssMessage.str("");
                  ssMessage << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno);
                  gpCentral->log(ssMessage.str(), strError);
                }
                delete[] fds;
                for (auto &i : removals)
                {
                  list<conn *>::iterator connsIter = conns.end();
                  for (auto j = conns.begin(); connsIter == conns.end() && j != conns.end(); j++)
                  {
                    if (i == (*j)->fdSocket)
                    {
                      connsIter = j;
                    }
                  }
                  if (connsIter != conns.end())
                  {
                    if ((*connsIter)->bRegistered)
                    {
                      if (acornDeregister(strPrefix, (*connsIter), strError))
                      {
                        ssMessage.str("");
                      }
                      else
                      {
                        ssMessage.str("");
                      }
                    }
                    close((*connsIter)->fdSocket);
                    delete (*connsIter);
                    conns.erase(connsIter);
                  }
                }
                removals.clear();
              }
              while (!conns.empty())
              {
                if (conns.front()->bRegistered)
                {
                  if (acornDeregister(strPrefix, conns.front(), strError))
                  {
                    ssMessage.str("");
                  }
                  else
                  {
                    ssMessage.str("");
                  }
                }
                close(conns.front()->fdSocket);
                delete conns.front();
                conns.pop_front();
              }
              gbShutdown = true;
            }
            else
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->listen(" << errno << ") error [" << UNIX_SOCKET << "]:  " << strerror(errno);
              gpCentral->log(ssMessage.str(), strError);
            }
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->bind(" << errno << ") error [" << UNIX_SOCKET << "]:  " << strerror(errno);
            gpCentral->log(ssMessage.str(), strError);
          }
          close(fdUnix);
        }
        else
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->socket(" << errno << ") error [" << UNIX_SOCKET << "]:  " << strerror(errno);
          gpCentral->log(ssMessage.str(), strError);
        }
        gpCentral->file()->remove(UNIX_SOCKET);
      }
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
// {{{ acornDeregister()
bool acornDeregister(string strPrefix, conn *ptConn, string &strError)
{
  bool bResult = false;
  list<string> in, out;
  string strJson;
  stringstream ssMessage;
  Json *ptRequest = new Json, *ptResponse = new Json;

  strPrefix += "->acornDeregister()";
  ptRequest->insert("Acorn", "router");
  ptRequest->insert("Password", gstrPassword);
  ptRequest->insert("Function", "deregister");
  ptRequest->insert("Server", gstrRouter);
  ptRequest->insert("reqApp", gstrApplication);
  ptRequest->m["Request"] = new Json;
  ptRequest->m["Request"]->insert("Gateway", gstrName);
  ptRequest->m["Request"]->insert("Acorn", ptConn->strName);
  if (gpCentral->acorn()->request(ptRequest, ptResponse, 5, strError))
  {
    if (ptResponse->m.find("Status") != ptResponse->m.end() && ptResponse->m["Status"]->v == "okay")
    {
      bResult = true;
      ssMessage.str("");
      ssMessage << strPrefix << "->Central::acorn()->request(router) [" << ptConn->strName << "]:  Deregistered.";
      gpCentral->log(ssMessage.str(), strError);
    }
    else if (ptResponse->m.find("Error") != ptResponse->m.end() && !ptResponse->m["Error"]->v.empty())
    {
      strError = ptResponse->m["Error"]->v;
      ssMessage.str("");
      ssMessage << strPrefix << "->Central::acorn()->request(router) error [" << ptConn->strName << "]:  " << strError;
      gpCentral->log(ssMessage.str(), strError);
    }
    else
    {
      strError = "Encountered an unknown error.";
      ssMessage.str("");
      ssMessage << strPrefix << "->Central::acorn()->request(router) error [" << ptConn->strName << "]:  " << strError;
      gpCentral->log(ssMessage.str(), strError);
    }
  }
  delete ptRequest;
  delete ptResponse;
  ptConn->bRegistered = false;
  ptConn->strName.clear();

  return bResult;
}
// }}}
// {{{ acornRegister()
bool acornRegister(string strPrefix, conn *ptConn, const string strName, string &strError)
{
  bool bResult = false;
  list<string> in, out;
  string strJson;
  stringstream ssMessage;
  Json *ptRequest = new Json, *ptResponse = new Json;

  strPrefix += "->acornRegister()";
  ptConn->bRegistered = true;
  ptConn->strName = strName;
  ptRequest->insert("Acorn", "router");
  ptRequest->insert("Password", gstrPassword);
  ptRequest->insert("Function", "register");
  ptRequest->insert("Server", gstrRouter);
  ptRequest->insert("reqApp", gstrApplication);
  ptRequest->m["Request"] = new Json;
  ptRequest->m["Request"]->insert("Gateway", gstrName);
  ptRequest->m["Request"]->insert("Acorn", ptConn->strName);
  if (gpCentral->acorn()->request(ptRequest, ptResponse, 5, strError))
  {
    if (ptResponse->m.find("Status") != ptResponse->m.end() && ptResponse->m["Status"]->v == "okay")
    {
      bResult = true;
      ssMessage.str("");
      ssMessage << strPrefix << "->Central::acorn()->request(router) [" << ptConn->strName << "]:  Registered.";
      gpCentral->log(ssMessage.str(), strError);
    }
    else if (ptResponse->m.find("Error") != ptResponse->m.end() && !ptResponse->m["Error"]->v.empty())
    {
      strError = ptResponse->m["Error"]->v;
      ssMessage.str("");
      ssMessage << strPrefix << "->Central::acorn()->request(router) error [" << ptConn->strName << "]:  " << strError;
      gpCentral->log(ssMessage.str(), strError);
    }
    else
    {
      strError = "Encountered an unknown error.";
      ssMessage.str("");
      ssMessage << strPrefix << "->Central::acorn()->request(router) error [" << ptConn->strName << "]:  " << strError;
      gpCentral->log(ssMessage.str(), strError);
    }
  }
  delete ptRequest;
  delete ptResponse;

  return bResult;
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
    sigignore(SIGBUS);
    sigignore(SIGCHLD);
    sigignore(SIGCONT);
    sigignore(SIGPIPE);
    sigignore(SIGSEGV);
    sigignore(SIGWINCH);
    // }}} 
    // {{{ command line arguments
    for (int i = 1; i < argc; i++)
    {   
      string strArg = argv[i];
      if (strArg == "-c" || (strArg.size() > 7 && strArg.substr(0, 7) == "--conf="))
      {
        string strConf;
        if (strArg == "-c" && i + 1 < argc && argv[i+1][0] != '-')
        {
          strConf = argv[++i];
        }
        else
        {
          strConf = strArg.substr(7, strArg.size() - 7);
        }
        gpCentral->manip()->purgeChar(strConf, strConf, "'");
        gpCentral->manip()->purgeChar(strConf, strConf, "\"");
        gpCentral->utility()->setConfPath(strConf, strError);
        gpCentral->acorn()->utility()->setConfPath(strConf, strError);
      }
      else if (strArg == "-d" || (strArg.size() > 7 && strArg.substr(0, 7) == "--data="))
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
      else if (strArg == "-n" || (strArg.size() > 7 && strArg.substr(0, 7) == "--name="))
      {
        if (strArg == "-n" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrName = argv[++i];
        }
        else
        {
          gstrName = strArg.substr(7, strArg.size() - 7);
        }
        gpCentral->manip()->purgeChar(gstrName, gstrName, "'");
        gpCentral->manip()->purgeChar(gstrName, gstrName, "\"");
      }
      else if (strArg == "-p" || (strArg.size() > 7 && strArg.substr(0, 7) == "--port="))
      {
        if (strArg == "-p" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrPort = argv[++i];
        }
        else
        {
          gstrPort = strArg.substr(7, strArg.size() - 7);
        }
        gpCentral->manip()->purgeChar(gstrPort, gstrPort, "'");
        gpCentral->manip()->purgeChar(gstrPort, gstrPort, "\"");
      }
      else if (strArg == "-r" || (strArg.size() > 9 && strArg.substr(0, 9) == "--router="))
      {
        if (strArg == "-r" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrRouter = argv[++i];
        }
        else
        {
          gstrRouter = strArg.substr(9, strArg.size() - 9);
        }
        gpCentral->manip()->purgeChar(gstrRouter, gstrRouter, "'");
        gpCentral->manip()->purgeChar(gstrRouter, gstrRouter, "\"");
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
    gpCentral->setLog(gstrData, "gateway_", "daily", true, true);
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
