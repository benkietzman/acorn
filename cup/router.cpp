// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : router.cpp
// author     : Ben Kietzman
// begin      : 2018-12-19
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

/*! \file router.cpp
* \brief Acorn Cup - router
*
* Provides the router cup for an acorn node.
*/
// {{{ includes
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
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
#include <Syslog>
using namespace common;
// }}}
// {{{ defines
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -n NAME, --name=NAME" << endl << "     Sets the router name." << endl << endl << " -s SERVER, --server=SERVER" << endl << "     Sets the server name." << endl << endl << "     --syslog" << endl << "     Enables syslog." << endl << endl
// }}}
// {{{ structs
struct conn
{
  addrinfo *result;
  bool bEnabled;
  int fdConnecting;
  int fdSocket;
  long lArg;
  list<string> acorns;
  sockaddr *addr;
  socklen_t addrlen;
  string strBuffer[2];
  string strName;
  string strPort;
  string strServer;
  Json *ptStatus;
};
// }}}
// {{{ global variables
bool gbShutdown = false; //!< Global shutdown variable.
list<conn *> gConns; //<! Contains the connections.
list<conn *> gRouters; //<! Contains the gRouters.
string gstrApplication = "Acorn"; //!< Global application name.
string gstrBuffer[2]; //!< Global buffers.
string gstrData = "/data/acorn"; //!< Global data directory.
string gstrName; //!< Global router name.
string gstrPassword; //<! Contains the password.
string gstrServer; //!< Global acorn or gateway name.
Central *gpCentral = NULL; //!< Contains the Central class.
Syslog *gpSyslog = NULL; //!< Contains the Syslog class.
// }}}
// {{{ prototypes
/*! \fn bool acornDeregister(string strPrefix, list<conn *>::iterator connIter, const string strAcorn, string &strError)
* \brief Deregister an acorn.
* \param strPrefix Contains the function prefix.
* \param connIter Contains the connection iterator.
* \param strAcorn Contains the acorn.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool acornDeregister(string strPrefix, list<conn *>::iterator connIter, const string strAcorn, string &strError);
/*! \fn bool acornRegister(string strPrefix, const string strAcorn, const string strName, const string strServer, const string strPort, string &strError)
* \brief Register an acorn.
* \param strPrefix Contains the function prefix.
* \param strAcorn Contains the acorn.
* \param strName Contains the gateway name.
* \param strServer Contains the gateway server.
* \param strPort Contains the gateway port.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool acornRegister(string strPrefix, const string strAcorn, const string strName, const string strServer, const string strPort, string &strError);
/*! \fn void initialize(string strPrefix, int argc, char *argv[], string &strError)
* \brief Monitors the health of the running process.
* \param strPrefix Contains the function prefix.
* \param argc Contains the number of command-line arguments.
* \param argv Contains the command-line arguments.
* \return Returns a boolean true/false value.
*/
bool initialize(string strPrefix, int argc, char *argv[], string &strError);
/*! \fn Json *status()
* \brief Retrieves the status of the router.
* \return Returns the status of the router.
*/
Json *status();
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
    ifstream inCred;
    stringstream ssCred;
    ssMessage.str("");
    ssMessage << strPrefix << ":  Initialized." << endl;
    gpCentral->log(ssMessage.str(), strError);
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
      addrinfo hints, *result;
      ifstream inRouters;
      int fdLink, nReturn;
      string strJson;
      stringstream ssRouters;
      Json *ptJson;
      ssMessage.str("");
      ssMessage << strPrefix << ":  Loaded router password." << endl;
      gpCentral->log(ssMessage.str(), strError);
      // {{{ load routers
      ssRouters << gstrData << "/routers";
      inRouters.open(ssRouters.str().c_str());
      if (inRouters)
      {
        string strRouter;
        while (getline(inRouters, strRouter))
        {
          conn *ptConn = new conn;
          ptConn->bEnabled = false;
          ptConn->fdSocket = -1;
          ptConn->strServer = strRouter;
          ptConn->ptStatus = NULL;
          memset(&hints, 0, sizeof(addrinfo));
          hints.ai_family = AF_UNSPEC;
          hints.ai_socktype = SOCK_STREAM;
          if ((nReturn = getaddrinfo(strRouter.c_str(), "22675", &hints, &result)) == 0)
          {
            bool bConnected[2] = {false, false};
            int fdSocket;
            addrinfo *rp;
            for (rp = result; !bConnected[1] && rp != NULL; rp = rp->ai_next)
            {
              bConnected[0] = false;
              if ((fdSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
              {
                bConnected[0] = true;
                if (connect(fdSocket, rp->ai_addr, rp->ai_addrlen) == 0)
                {
                  bConnected[1] = true;
                }
                else
                {
                  close(fdSocket);
                }
              }
            }
            freeaddrinfo(result);
            if (bConnected[1])
            {
              ptConn->fdSocket = fdSocket;
              ssMessage.str("");
              ssMessage << strPrefix << "->connect() [" << strRouter << "]:  Connected to router." << endl;
              gpCentral->log(ssMessage.str(), strError);
              ptJson = new Json;
              ptJson->insert("Password", gstrPassword);
              ptJson->insert("Server", gstrServer);
              ptConn->strBuffer[1].append(ptJson->json(strJson)+"\n");
              delete ptJson;
            }
            else
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->" << ((!bConnected[0])?"socket":"connect") << "(" << errno << ") error  [" << strRouter << "]:  " << strerror(errno) << endl;
              gpCentral->log(ssMessage.str(), strError);
            }
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->getaddrinfo(" << nReturn << ") error  [" << strRouter << "]:  " << gai_strerror(nReturn) << endl;
            gpCentral->log(ssMessage.str(), strError);
          }
          gRouters.push_back(ptConn);
        }
      }
      inRouters.close();
      // }}}
      memset(&hints, 0, sizeof(addrinfo));
      hints.ai_family = AF_INET6;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;
      if ((nReturn = getaddrinfo(NULL, "22675", &hints, &result)) == 0)
      {
        addrinfo *rp;
        bool bBound[2] = {false, false};
        for (rp = result; !bBound[1] && rp != NULL; rp = rp->ai_next)
        {
          bBound[0] = false;
          if ((fdLink = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
          {
            int nOn = 1;
            bBound[0] = true;
            setsockopt(fdLink, SOL_SOCKET, SO_REUSEADDR, (char *)&nOn, sizeof(int));
            if (bind(fdLink, rp->ai_addr, rp->ai_addrlen) == 0)
            {
              bBound[1] = true;
            }
            else
            {
              close(fdLink);
            }
          }
        }
        freeaddrinfo(result);
        if (bBound[1])
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->bind():  Bound to link socket." << endl;
          gpCentral->log(ssMessage.str(), strError);
          if (listen(fdLink, 5) == 0)
          {
            bool bExit = false;
            char szBuffer[65536];
            list<conn *> links;
            list<int> linkRemovals;
            list<string> removals;
            list<list<conn *>::iterator> routerClosures;
            map<string, time_t> gateways;
            pollfd *fds;
            size_t unIndex, unPosition;
            time_t CTime[2];
            ssMessage.str("");
            ssMessage << strPrefix << "->bind():  Listening to link socket." << endl;
            gpCentral->log(ssMessage.str(), strError);
            time(&(CTime[0]));
            while (!gbShutdown && !bExit)
            {
              // {{{ prep sockets
              unIndex = ((!gstrBuffer[1].empty())?3:2);
              for (list<conn *>::iterator i = gConns.begin(); i != gConns.end(); i++)
              {
                if ((*i)->fdSocket != -1)
                {
                  unIndex++;
                }
                else if ((*i)->fdConnecting != -1)
                {
                  if (connect((*i)->fdConnecting, (*i)->addr, (*i)->addrlen) == 0)
                  {
                    unIndex++;
                    (*i)->fdSocket = (*i)->fdConnecting;
                    (*i)->fdConnecting = -1;
                    if ((*i)->lArg >= 0)
                    {
                      fcntl((*i)->fdSocket, F_SETFL, (*i)->lArg);
                    }
                    ssMessage.str("");
                    ssMessage << strPrefix << "->connect() [" << (*i)->strName << "]:  Connected to gateway.";
                    gpCentral->log(ssMessage.str(), strError);
                  }
                  else if (errno != EALREADY && errno != EINPROGRESS)
                  {
                    close((*i)->fdConnecting);
                    freeaddrinfo((*i)->result);
                    removals.push_back((*i)->strServer);
                  }
                }
                else
                {
                  bool bConnected[2] = {false, false};
                  memset(&hints, 0, sizeof(addrinfo));
                  hints.ai_family = AF_UNSPEC;
                  hints.ai_socktype = SOCK_STREAM;
                  if ((nReturn = getaddrinfo((*i)->strServer.c_str(), (*i)->strPort.c_str(), &hints, &((*i)->result))) == 0)
                  {
                    addrinfo *rp;
                    for (rp = ((*i)->result); !bConnected[1] && rp != NULL; rp = rp->ai_next)
                    {
                      bConnected[0] = false;
                      if (((*i)->fdConnecting = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
                      {
                        long lArg;
                        (*i)->addr = rp->ai_addr;
                        (*i)->addrlen = rp->ai_addrlen;
                        if (((*i)->lArg = lArg = fcntl((*i)->fdConnecting, F_GETFL, NULL)) >= 0)
                        {
                          lArg |= O_NONBLOCK;
                          fcntl((*i)->fdConnecting, F_SETFL, lArg);
                        }
                        if (connect((*i)->fdConnecting, (*i)->addr, (*i)->addrlen) == 0)
                        {
                          unIndex++;
                          (*i)->fdSocket = (*i)->fdConnecting;
                          (*i)->fdConnecting = -1;
                          if ((*i)->lArg >= 0)
                          {
                            fcntl((*i)->fdSocket, F_SETFL, (*i)->lArg);
                          }
                          ssMessage.str("");
                          ssMessage << strPrefix << "->connect() [" << (*i)->strName << "]:  Connected to gateway.";
                          gpCentral->log(ssMessage.str(), strError);
                        }
                        else if (errno != EALREADY && errno != EINPROGRESS)
                        {
                          close((*i)->fdConnecting);
                          freeaddrinfo((*i)->result);
                          removals.push_back((*i)->strServer);
                        }
                      }
                      else
                      {
                        freeaddrinfo((*i)->result);
                        removals.push_back((*i)->strServer);
                      }
                    }
                  }
                  else
                  {
                    removals.push_back((*i)->strServer);
                  }
                }
              }
              unIndex += links.size();
              for (list<conn *>::iterator i = gRouters.begin(); i != gRouters.end(); i++)
              {
                if ((*i)->fdSocket != -1)
                {
                  unIndex++;
                }
              }
              fds = new pollfd[unIndex];
              unIndex = 0;
              fds[unIndex].fd = 0;
              fds[unIndex].events = POLLIN;
              unIndex++;
              if (!gstrBuffer[1].empty())
              {
                fds[unIndex].fd = 1;
                fds[unIndex].events = POLLOUT;
                unIndex++;
              }
              fds[unIndex].fd = fdLink;
              fds[unIndex].events = POLLIN;
              unIndex++;
              for (list<conn *>::iterator i = gConns.begin(); i != gConns.end(); i++)
              {
                if ((*i)->fdSocket != -1)
                {
                  fds[unIndex].fd = (*i)->fdSocket;
                  fds[unIndex].events = POLLIN;
                  if (!(*i)->strBuffer[1].empty())
                  {
                    fds[unIndex].events |= POLLOUT;
                  }
                  unIndex++;
                }
              }
              for (list<conn *>::iterator i = links.begin(); i != links.end(); i++)
              {
                fds[unIndex].fd = (*i)->fdSocket;
                fds[unIndex].events = POLLIN;
                unIndex++;
              }
              for (list<conn *>::iterator i = gRouters.begin(); i != gRouters.end(); i++)
              {
                if ((*i)->fdSocket != -1)
                {
                  fds[unIndex].fd = (*i)->fdSocket;
                  fds[unIndex].events = POLLIN;
                  if (!(*i)->strBuffer[1].empty())
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
                  // {{{ accept incoming links
                  if (fds[i].fd == fdLink)
                  {
                    if (fds[i].revents & POLLIN)
                    {
                      int fdClient;
                      socklen_t clilen;
                      sockaddr_in cli_addr;
                      clilen = sizeof(sockaddr_in);
                      if ((fdClient = accept(fdLink, (sockaddr *)&cli_addr, &clilen)) >= 0)
                      {
                        conn *ptConn = new conn;
                        if (gpSyslog != NULL)
                        {
                          gpSyslog->connectionStarted("Accepted an incoming request.", fdClient);
                        }
                        ptConn->fdSocket = fdClient;
                        links.push_back(ptConn);
                      }
                    }
                  }
                  // }}}
                  // {{{ routers
                  for (list<conn *>::iterator j = gRouters.begin(); j != gRouters.end(); j++)
                  {
                    if (fds[i].fd == (*j)->fdSocket)
                    {
                      // {{{ read from router
                      if (fds[i].revents & POLLIN)
                      {
                        if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                        {
                          (*j)->strBuffer[0].append(szBuffer, nReturn);
                          while ((unPosition = (*j)->strBuffer[0].find("\n")) != string::npos)
                          {
                            Json *ptRequest = new Json((*j)->strBuffer[0].substr(0, unPosition));
                            (*j)->strBuffer[0].erase(0, (unPosition + 1));
                            if (ptRequest->m.find("Password") != ptRequest->m.end() && ptRequest->m["Password"]->v == gstrPassword)
                            {
                              if (gpSyslog != NULL)
                              {
                                gpSyslog->logon("Authenticated the router.");
                              }
                              if (ptRequest->m.find("Function") != ptRequest->m.end() && !ptRequest->m["Function"]->v.empty())
                              {
                                if (ptRequest->m.find("Request") != ptRequest->m.end())
                                {
                                  // {{{ deregister, register
                                  if (ptRequest->m["Function"]->v == "deregister" || ptRequest->m["Function"]->v == "register")
                                  {
                                    if (ptRequest->m["Request"]->m.find("Gateway") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Gateway"]->v.empty())
                                    {
                                      list<conn *>::iterator connIter = gConns.end();
                                      string strAcorn;
                                      if (ptRequest->m["Request"]->m.find("Acorn") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Acorn"]->v.empty())
                                      { 
                                        strAcorn = ptRequest->m["Request"]->m["Acorn"]->v;
                                      }
                                      for (list<conn *>::iterator k = gConns.begin(); connIter == gConns.end() && k != gConns.end(); k++)
                                      {
                                        if ((*k)->strName == ptRequest->m["Request"]->m["Gateway"]->v)
                                        {
                                          if (!strAcorn.empty())
                                          {
                                            for (list<string>::iterator l = (*k)->acorns.begin(); connIter == gConns.end() && l != (*k)->acorns.end(); l++)
                                            {
                                              if ((*l) == strAcorn)
                                              { 
                                                connIter = k;
                                              }
                                            }
                                          }
                                          else
                                          { 
                                            connIter = k;
                                          }
                                        }
                                      }
                                      if (ptRequest->m["Function"]->v == "deregister")
                                      {
                                        if (connIter != gConns.end())
                                        {
                                          acornDeregister(strPrefix, connIter, strAcorn, strError);
                                        }
                                      }
                                      else
                                      {
                                        string strPort, strServer;
                                        if (ptRequest->m["Request"]->m.find("Port") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Port"]->v.empty())
                                        {
                                          strPort = ptRequest->m["Request"]->m["Port"]->v;
                                        }
                                        if (ptRequest->m["Request"]->m.find("Server") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Server"]->v.empty())
                                        {
                                          strServer = ptRequest->m["Request"]->m["Server"]->v;
                                        }
                                        acornRegister(strPrefix, strAcorn, ptRequest->m["Request"]->m["Gateway"]->v, strServer, strPort, strError);
                                      }
                                    }
                                    else
                                    {
                                      ssMessage.str("");
                                      ssMessage << strPrefix << " error:  Please provide the Gateway within the Request.";
                                      gpCentral->log(ssMessage.str(), strError);
                                    }
                                  }
                                  // }}}
                                  // {{{ status
                                  else if (ptRequest->m["Function"]->v == "status")
                                  {
                                    if ((*j)->ptStatus != NULL)
                                    {
                                      delete (*j)->ptStatus;
                                    }
                                    (*j)->ptStatus = new Json(ptRequest->m["Request"]);
                                  }
                                  // }}}
                                  // {{{ invalid
                                  else
                                  {
                                    ssMessage.str("");
                                    ssMessage << strPrefix << " error:  Please provide a valid Function:  deregister, register, status.";
                                    gpCentral->log(ssMessage.str(), strError);
                                  }
                                  // }}}
                                }
                                else
                                {
                                  ssMessage.str("");
                                  ssMessage << strPrefix << " error:  Please provide the Request.";
                                  gpCentral->log(ssMessage.str(), strError);
                                }
                              }
                              else
                              {
                                ssMessage.str("");
                                ssMessage << strPrefix << " error:  Please provide the Function.";
                                gpCentral->log(ssMessage.str(), strError);
                              }
                            }
                            else
                            {
                              ssMessage.str("");
                              ssMessage << strPrefix << " error:  Access denied.";
                              gpCentral->log(ssMessage.str(), strError);
                            }
                            delete ptRequest;
                          }
                        }
                        else
                        {
                          routerClosures.push_back(j);
                          if (nReturn < 0)
                          {
                            ssMessage.str("");
                            ssMessage << strPrefix << "->read(" << errno << ") error:  " << strerror(errno);
                            gpCentral->log(ssMessage.str(), strError);
                          }
                        }
                      }
                      // }}}
                      // {{{ write to router
                      if (fds[i].revents & POLLOUT)
                      {
                        if ((nReturn = write(fds[i].fd, (*j)->strBuffer[1].c_str(), (*j)->strBuffer[1].size())) > 0)
                        {
                          (*j)->strBuffer[1].erase(0, nReturn);
                        }
                        else
                        {
                          routerClosures.push_back(j);
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
                  // {{{ links
                  for (list<conn *>::iterator j = links.begin(); j != links.end(); j++)
                  {
                    if (fds[i].fd == (*j)->fdSocket)
                    {
                      // {{{ read from link
                      if (fds[i].revents & POLLIN)
                      {
                        if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                        {
                          (*j)->strBuffer[0].append(szBuffer, nReturn);
                          if ((unPosition = (*j)->strBuffer[0].find("\n")) != string::npos)
                          {
                            ptJson = new Json((*j)->strBuffer[0].substr(0, unPosition));
                            (*j)->strBuffer[0].erase(0, (unPosition + 1));
                            if (ptJson->m.find("Password") != ptJson->m.end() && ptJson->m["Password"]->v == gstrPassword)
                            {
                              if (gpSyslog != NULL)
                              {
                                gpSyslog->logon("Authenticated the link.");
                              }
                              if (ptJson->m.find("Server") != ptJson->m.end() && !ptJson->m["Server"]->v.empty())
                              {
                                list<conn *>::iterator connIter = gRouters.end();
                                for (list<conn *>::iterator k = gRouters.begin(); connIter == gRouters.end() && k != gRouters.end(); k++)
                                {
                                  if ((*k)->strServer == ptJson->m["Server"]->v)
                                  {
                                    connIter = k;
                                  }
                                }
                                if (connIter != gRouters.end())
                                {
                                  ssMessage.str("");
                                  ssMessage << strPrefix << "->read() [" << ptJson->m["Server"]->v << "]:  Connected to router.";
                                  gpCentral->log(ssMessage.str(), strError);
                                  (*connIter)->fdSocket = (*j)->fdSocket;
                                  (*j)->fdSocket = -1;
                                  (*connIter)->strServer = ptJson->m["Server"]->v;
                                  (*connIter)->strBuffer[0] = (*j)->strBuffer[0];
                                  for (list<conn *>::iterator k = gConns.begin(); k != gConns.end(); k++)
                                  {
                                    Json *ptRequest = new Json;
                                    ptRequest->insert("Password", gstrPassword);
                                    ptRequest->insert("Function", "register");
                                    ptRequest->m["Request"] = new Json;
                                    ptRequest->m["Request"]->insert("Acorn", "gateway");
                                    ptRequest->m["Request"]->insert("Gateway", (*k)->strName);
                                    ptRequest->m["Request"]->insert("Server", (*k)->strServer);
                                    ptRequest->m["Request"]->insert("Port", (*k)->strPort);
                                    (*connIter)->strBuffer[1].append(ptRequest->json(strJson)+"\n");
                                    delete ptRequest;
                                    ssMessage.str("");
                                    ssMessage << strPrefix << "->read() [" << ptJson->m["Server"]->v << "," << (*k)->strName << "]:  Sent gateway registration.";
                                    gpCentral->log(ssMessage.str(), strError);
                                    for (list<string>::iterator l = (*k)->acorns.begin(); l != (*k)->acorns.end(); l++)
                                    {
                                      ptRequest = new Json;
                                      ptRequest->insert("Password", gstrPassword);
                                      ptRequest->insert("Function", "register");
                                      ptRequest->m["Request"] = new Json;
                                      ptRequest->m["Request"]->insert("Acorn", (*l));
                                      ptRequest->m["Request"]->insert("Gateway", (*k)->strName);
                                      (*connIter)->strBuffer[1].append(ptRequest->json(strJson)+"\n");
                                      delete ptRequest;
                                      ssMessage.str("");
                                      ssMessage << strPrefix << "->read() [" << ptJson->m["Server"]->v << "," << (*k)->strName << "," << (*l) << "]:  Sent registration.";
                                      gpCentral->log(ssMessage.str(), strError);
                                    }
                                  }
                                }
                              }
                            }
                            delete ptJson;
                            linkRemovals.push_back((*j)->fdSocket);
                          }
                        }
                        else
                        {
                          linkRemovals.push_back((*j)->fdSocket);
                          if (nReturn < 0)
                          {
                            ssMessage.str("");
                            ssMessage << strPrefix << "->read(" << errno << ") error:  " << strerror(errno);
                            gpCentral->log(ssMessage.str(), strError);
                          }
                        }
                      }
                      // }}}
                    }
                  }
                  // }}}
                  // {{{ read from cap
                  if (fds[i].fd == 0)
                  {
                    if (fds[i].revents & POLLIN)
                    {
                      if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                      {
                        gstrBuffer[0].append(szBuffer, nReturn);
                        while ((unPosition = gstrBuffer[0].find("\n")) != string::npos)
                        {
                          Json *ptRequest = new Json(gstrBuffer[0].substr(0, unPosition));
                          gstrBuffer[0].erase(0, (unPosition + 1));
                          if (ptRequest->m.find("Acorn") != ptRequest->m.end() && !ptRequest->m["Acorn"]->v.empty())
                          {
                            if (ptRequest->m["Acorn"]->v == "router")
                            {
                              if (ptRequest->m.find("Function") != ptRequest->m.end() && !ptRequest->m["Function"]->v.empty())
                              {
                                // {{{ deregister | register
                                if (ptRequest->m["Function"]->v == "deregister" || ptRequest->m["Function"]->v == "register")
                                {
                                  if (ptRequest->m.find("Password") != ptRequest->m.end() && !ptRequest->m["Password"]->v.empty())
                                  {
                                    if (ptRequest->m["Password"]->v == gstrPassword)
                                    {
                                      if (gpSyslog != NULL)
                                      {
                                        gpSyslog->logon("Authenticated the client.");
                                      }
                                      if (ptRequest->m.find("Request") != ptRequest->m.end())
                                      {
                                        if (ptRequest->m["Request"]->m.find("Gateway") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Gateway"]->v.empty())
                                        {
                                          list<conn *>::iterator connIter = gConns.end();
                                          string strAcorn;
                                          if (ptRequest->m["Request"]->m.find("Acorn") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Acorn"]->v.empty())
                                          {
                                            strAcorn = ptRequest->m["Request"]->m["Acorn"]->v;
                                          }
                                          for (list<conn *>::iterator j = gConns.begin(); connIter == gConns.end() && j != gConns.end(); j++)
                                          {
                                            if ((*j)->strName == ptRequest->m["Request"]->m["Gateway"]->v)
                                            {
                                              if (!strAcorn.empty())
                                              {
                                                for (list<string>::iterator k = (*j)->acorns.begin(); connIter == gConns.end() && k != (*j)->acorns.end(); k++)
                                                {
                                                  if ((*k) == strAcorn)
                                                  {
                                                    connIter = j;
                                                  }
                                                }
                                              }
                                              else
                                              {
                                                connIter = j;
                                              }
                                            }
                                          }
                                          if (ptRequest->m["Function"]->v == "deregister")
                                          {
                                            if (connIter != gConns.end())
                                            {
                                              if (acornDeregister(strPrefix, connIter, strAcorn, strError))
                                              {
                                                ptRequest->insert("Status", "okay");
                                                gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                              }
                                              else
                                              {
                                                ptRequest->insert("Status", "error");
                                                ptRequest->insert("Error", strError);
                                                gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                              }
                                            }
                                          }
                                          else
                                          {
                                            string strPort, strServer;
                                            if (ptRequest->m["Request"]->m.find("Port") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Port"]->v.empty())
                                            {
                                              strPort = ptRequest->m["Request"]->m["Port"]->v;
                                            }
                                            if (ptRequest->m["Request"]->m.find("Server") != ptRequest->m["Request"]->m.end() && !ptRequest->m["Request"]->m["Server"]->v.empty())
                                            {
                                              strServer = ptRequest->m["Request"]->m["Server"]->v;
                                            }
                                            if (acornRegister(strPrefix, strAcorn, ptRequest->m["Request"]->m["Gateway"]->v, strServer, strPort, strError))
                                            {
                                              ptRequest->insert("Status", "okay");
                                              gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                            }
                                            else if (strError.find(" is already registered.") == string::npos)
                                            {
                                              ptRequest->insert("Status", "error");
                                              ptRequest->insert("Error", strError);
                                              gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                            }
                                          }
                                        }
                                        else
                                        {
                                          ptRequest->insert("Status", "error");
                                          ptRequest->insert("Error", "Please provide the Gateway within the Request.");
                                          gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                        }
                                      }
                                      else
                                      {
                                        ptRequest->insert("Status", "error");
                                        ptRequest->insert("Error", "Please provide the Request.");
                                        gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                      }
                                    }
                                    else
                                    {
                                      ptRequest->insert("Status", "error");
                                      ptRequest->insert("Error", "Access denied.");
                                      gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                    }
                                  }
                                  else
                                  {
                                    ptRequest->insert("Status", "error");
                                    ptRequest->insert("Error", "Please provide the Password.");
                                    gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                  }
                                }
                                // }}}
                                // {{{ list
                                else if (ptRequest->m["Function"]->v == "list")
                                {
                                  list<string> acorns;
                                  ptRequest->insert("Status", "okay");
                                  if (ptRequest->m.find("Response") != ptRequest->m.end())
                                  {
                                    delete ptRequest->m["Response"];
                                  }
                                  ptRequest->m["Response"] = new Json;
                                  for (list<conn *>::iterator j = gConns.begin(); j != gConns.end(); j++)
                                  {
                                    if ((*j)->bEnabled)
                                    {
                                      for (list<string>::iterator k = (*j)->acorns.begin(); k != (*j)->acorns.end(); k++)
                                      {
                                        acorns.push_back(*k);
                                      }
                                    }
                                  }
                                  acorns.sort();
                                  acorns.unique();
                                  ptRequest->m["Response"]->insert(acorns);
                                  acorns.clear();
                                  gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                }
                                // }}}
                                // {{{ ping
                                else if (ptRequest->m["Function"]->v == "ping")
                                {
                                  ptRequest->insert("Status", "okay");
                                  gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                }
                                // }}}
                                // {{{ status
                                else if (ptRequest->m["Function"]->v == "status")
                                {
                                  ptRequest->insert("Status", "okay");
                                  if (ptRequest->m.find("Response") != ptRequest->m.end())
                                  {
                                    delete ptRequest->m["Response"];
                                  }
                                  ptRequest->m["Response"] = new Json;
                                  ptRequest->m["Response"]->l.push_back(status());
                                  for (list<conn *>::iterator j = gRouters.begin(); j != gRouters.end(); j++)
                                  {
                                    if ((*j)->ptStatus != NULL)
                                    {
                                      ptRequest->m["Response"]->push_back((*j)->ptStatus);
                                    }
                                  }
                                  gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                }
                                // }}}
                                // {{{ invalid
                                else
                                {
                                  ptRequest->insert("Status", "error");
                                  ptRequest->insert("Error", "Please provide a valid Function:  deregister, list, ping, register, status.");
                                  gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                                }
                                // }}}
                              }
                              else
                              {
                                ptRequest->insert("Status", "error");
                                ptRequest->insert("Error", "Please provide the Function.");
                                gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                              }
                            }
                            else
                            {
                              list<conn *> conns;
                              for (list<conn *>::iterator j = gConns.begin(); j != gConns.end(); j++)
                              {
                                if ((*j)->bEnabled)
                                {
                                  bool bFound = false;
                                  for (list<string>::iterator k = (*j)->acorns.begin(); !bFound && k != (*j)->acorns.end(); k++)
                                  {
                                    if ((*k) == ptRequest->m["Acorn"]->v)
                                    {
                                      bFound = true;
                                      conns.push_back(*j);
                                    }
                                  }
                                }
                              }
                              if (!conns.empty())
                              {
                                list<conn *>::iterator connIter = conns.begin();
                                unsigned int unPick, unSeed = time(NULL);
                                srand(unSeed);
                                unPick = rand_r(&unSeed) % conns.size();
                                for (size_t j = 0; j < unPick; j++)
                                {
                                  connIter++;
                                }
                                (*connIter)->strBuffer[1].append(ptRequest->json(strJson)+"\n");
                              }
                              else
                              {
                                ptRequest->insert("Status", "error");
                                ptRequest->insert("Error", "Please provide a valid Acorn.");
                                gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
                              }
                              conns.clear();
                            }
                          }
                          else
                          {
                            ptRequest->insert("Status", "error");
                            ptRequest->insert("Error", "Please provide the Acorn.");
                            gstrBuffer[1].append(ptRequest->json(strJson)+"\n");
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
                      if ((nReturn = write(fds[i].fd, gstrBuffer[1].c_str(), gstrBuffer[1].size())) > 0)
                      {
                        gstrBuffer[1].erase(0, nReturn);
                      }
                      else
                      {
                        bExit = true;
                        if (nReturn < 0)
                        {
                          ssMessage.str("");
                          ssMessage << strPrefix << "->write(" << errno << ") error:  " << strerror(errno);
                          gpCentral->log(ssMessage.str(), strError);
                        }
                      }
                    }
                  }
                  // }}}
                  // {{{ downstream clients
                  for (list<conn *>::iterator j = gConns.begin(); j != gConns.end(); j++)
                  {
                    if (fds[i].fd == (*j)->fdSocket)
                    {
                      // {{{ read from client
                      if (fds[i].revents & POLLIN)
                      {
                        if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                        {
                          (*j)->strBuffer[0].append(szBuffer, nReturn);
                          while ((unPosition = (*j)->strBuffer[0].find("\n")) != string::npos)
                          {
                            ptJson = new Json((*j)->strBuffer[0].substr(0, unPosition));
                            if (ptJson->m.find("_AcornPing") != ptJson->m.end())
                            {
                              bool bEnabled = false;
                              if (ptJson->m.find("Status") != ptJson->m.end() && ptJson->m["Status"]->v == "okay")
                              {
                                bEnabled = true;
                              }
                              else if (ptJson->m.find("Error") != ptJson->m.end() && !ptJson->m["Error"]->v.empty())
                              {
                                strError = ptJson->m["Error"]->v;
                              }
                              else
                              {
                                strError = "Encountered an unknown error.";
                              }
                              if (gateways.find((*j)->strName) != gateways.end())
                              {
                                gateways.erase((*j)->strName);
                              }
                              if ((*j)->bEnabled != bEnabled)
                              {
                                (*j)->bEnabled = bEnabled;
                                ssMessage.str("");
                                ssMessage << strPrefix << ((bEnabled)?"":" error") << " [" << gstrName << "," << (*j)->strName << "]:  " << ((bEnabled)?"Enabled":"Disabled") << " gateway.";
                                if (!bEnabled)
                                {
                                  ssMessage << "  " << strError;
                                }
                                gpCentral->log(ssMessage.str(), strError);
                              }
                            }
                            else
                            {
                              gstrBuffer[1].append((*j)->strBuffer[0].substr(0, (unPosition + 1)));
                            }
                            (*j)->strBuffer[0].erase(0, (unPosition + 1));
                            delete ptJson;
                          }
                        }
                        else
                        {
                          removals.push_back((*j)->strServer);
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
                        if ((nReturn = write(fds[i].fd, (*j)->strBuffer[1].c_str(), (*j)->strBuffer[1].size())) > 0)
                        {
                          (*j)->strBuffer[1].erase(0, nReturn);
                        }
                        else
                        {
                          removals.push_back((*j)->strServer);
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
              // {{{ remove links
              for (list<int>::iterator i = linkRemovals.begin(); i != linkRemovals.end(); i++)
              {
                list<conn *>::iterator linksIter = links.end();
                for (list<conn *>::iterator j = links.begin(); linksIter == links.end() && j != links.end(); j++)
                {
                  if ((*i) == (*j)->fdSocket)
                  {
                    linksIter = j;
                  }
                }
                if (linksIter != links.end())
                {
                  if ((*linksIter)->fdSocket != -1)
                  {
                    close((*linksIter)->fdSocket);
                  }
                  delete (*linksIter);
                  links.erase(linksIter);
                }
              }
              linkRemovals.clear();
              // }}}
              // {{{ remove connections
              for (list<string>::iterator i = removals.begin(); i != removals.end(); i++)
              {
                list<conn *>::iterator connsIter = gConns.end();
                for (list<conn *>::iterator j = gConns.begin(); connsIter == gConns.end() && j != gConns.end(); j++)
                {
                  if ((*i) == (*j)->strServer)
                  {
                    connsIter = j;
                  }
                }
                if (connsIter != gConns.end())
                {
                  acornDeregister(strPrefix, connsIter, "gateway", strError);
                }
              }
              removals.clear();
              // }}}
              // {{{ close routers
              for (list<list<conn *>::iterator>::iterator i = routerClosures.begin(); i != routerClosures.end(); i++)
              {
                if ((*(*i))->fdSocket != -1)
                {
                  ssMessage.str("");
                  ssMessage << strPrefix << "->close() [" << (*(*i))->strServer << "]:  Disconnected from router." << endl;
                  gpCentral->log(ssMessage.str(), strError);
                  close((*(*i))->fdSocket);
                  (*(*i))->fdSocket = -1;
                }
                (*(*i))->strBuffer[0].clear();
                (*(*i))->strBuffer[1].clear();
                if ((*(*i))->ptStatus != NULL)
                {
                  delete (*(*i))->ptStatus;
                  (*(*i))->ptStatus = NULL;
                }
              }
              routerClosures.clear();
              // }}}
              time(&(CTime[1]));
              if ((CTime[1] - CTime[0]) >= 10)
              {
                CTime[0] = CTime[1];
                // {{{ send status
                ptJson = new Json;
                ptJson->insert("Password", gstrPassword);
                ptJson->insert("Function", "status");
                ptJson->m["Request"] = status();
                for (list<conn *>::iterator i = gRouters.begin(); i != gRouters.end(); i++)
                {
                  if ((*i)->fdSocket != -1)
                  {
                    (*i)->strBuffer[1].append(ptJson->json(strJson)+"\n");
                  }
                }
                delete ptJson;
                // }}}
                // {{{ ping gateways
                for (list<conn *>::iterator i = gConns.begin(); i != gConns.end(); i++)
                {
                  if ((*i)->fdSocket != -1)
                  {
                    if (gateways.find((*i)->strName) != gateways.end())
                    {
                      if ((CTime[1] - gateways[(*i)->strName]) > 5)
                      {
                        gateways.erase((*i)->strName);
                        if ((*i)->bEnabled)
                        {
                          (*i)->bEnabled = false;
                          ssMessage.str("");
                          ssMessage << strPrefix << " error [" << gstrName << "," << (*i)->strName << "]:  Disabled gateway.  Timeout expired.";
                          gpCentral->log(ssMessage.str(), strError);
                        }
                      }
                    }
                    if (gateways.find((*i)->strName) == gateways.end())
                    {
                      ptJson = new Json;
                      gateways[(*i)->strName] = CTime[1];
                      ptJson->insert("_AcornPing", (*i)->strName);
                      ptJson->insert("_AcornRouter", gstrName);
                      ptJson->insert("Acorn", "ping");
                      ptJson->insert("reqApp", gstrApplication);
                      (*i)->strBuffer[1].append(ptJson->json(strJson)+"\n");
                      delete ptJson;
                    }
                  }
                }
                // }}}
              }
            }
            // {{{ remove connections
            while (!gConns.empty())
            {
              acornDeregister(strPrefix, gConns.begin(), "gateway", strError);
            }
            // }}}
            // {{{ remove links
            while (!links.empty())
            {
              close(links.front()->fdSocket);
              delete links.front();
              links.pop_front();
            }
            // }}}
            // {{{ remove routers
            while (!gRouters.empty())
            {
              if (gRouters.front()->fdSocket != -1)
              {
                ssMessage.str("");
                ssMessage << strPrefix << "->close() [" << gRouters.front()->strServer << "]:  Disconnected from router." << endl;
                gpCentral->log(ssMessage.str(), strError);
                close(gRouters.front()->fdSocket);
              }
              if (gRouters.front()->ptStatus != NULL)
              {
                delete gRouters.front()->ptStatus;
              }
              delete gRouters.front();
              gRouters.pop_front();
            }
            // }}}
            gateways.clear();
            gbShutdown = true;
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->listen(" << errno << ") error:  " << strerror(errno);
            gpCentral->log(ssMessage.str(), strError);
          }
          close(fdLink);
        }
        else
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->" << ((!bBound[0])?"socket":"bind") << "(" << errno << ") error:  " << strerror(errno);
          gpCentral->log(ssMessage.str(), strError);
        }
      }
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->getaddrinfo(" << nReturn << ") error:  " << gai_strerror(nReturn);
        gpCentral->log(ssMessage.str(), strError);
      }
    }
    if (gpSyslog != NULL)
    {
      delete gpSyslog;
    }
    delete gpCentral;
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
bool acornDeregister(string strPrefix, list<conn *>::iterator connIter, const string strAcorn, string &strError)
{
  bool bResult = false;
  string strJson;
  stringstream ssMessage;
  Json *ptJson;

  strPrefix += "->acornRegister()";
  if (connIter != gConns.end())
  {
    if (strAcorn != "gateway")
    {
      list<string>::iterator acornIter = (*connIter)->acorns.end();
      for (list<string>::iterator i = (*connIter)->acorns.begin(); acornIter == (*connIter)->acorns.end() && i != (*connIter)->acorns.end(); i++)
      {
        if ((*i) == strAcorn)
        {
          acornIter = i;
        }
      }
      if (acornIter != (*connIter)->acorns.end())
      {
        bResult = true;
        ssMessage.str("");
        ssMessage << strPrefix << "[" << (*connIter)->strName << "," << (*acornIter) << "]:  Deregistered.";
        gpCentral->log(ssMessage.str(), strError);
        (*connIter)->acorns.erase(acornIter);
        for (list<conn *>::iterator i = gRouters.begin(); i != gRouters.end(); i++)
        {
          if ((*i)->fdSocket != -1)
          {
            ptJson = new Json;
            ptJson->insert("Password", gstrPassword);
            ptJson->insert("Function", "deregister");
            ptJson->m["Request"] = new Json;
            ptJson->m["Request"]->insert("Gateway", (*connIter)->strName);
            ptJson->m["Request"]->insert("Acorn", strAcorn);
            (*i)->strBuffer[1].append(ptJson->json(strJson)+"\n");
            delete ptJson;
          }
        }
      }
      else
      {
        strError = "Please provide a valid acorn.";
        ssMessage.str("");
        ssMessage << strPrefix << " error [" << (*connIter)->strName << "," << strAcorn << "]:  " << strError;
        gpCentral->log(ssMessage.str(), strError);
      }
    }
    else
    {
      bResult = true;
      if ((*connIter)->fdSocket != -1)
      {
        while (!(*connIter)->acorns.empty())
        {
          ssMessage.str("");
          ssMessage << strPrefix << "[" << (*connIter)->strName << "," << (*connIter)->acorns.front() << "]:  Deregistered.";
          gpCentral->log(ssMessage.str(), strError);
          (*connIter)->acorns.pop_front();
        }
        close((*connIter)->fdSocket);
        ssMessage.str("");
        ssMessage << strPrefix << "[" << (*connIter)->strName << "]:  Disconnected gateway.";
        gpCentral->log(ssMessage.str(), strError);
      }
      (*connIter)->acorns.clear();
      for (list<conn *>::iterator i = gRouters.begin(); i != gRouters.end(); i++)
      {
        if ((*i)->fdSocket != -1)
        {
          ptJson = new Json;
          ptJson->insert("Password", gstrPassword);
          ptJson->insert("Function", "deregister");
          ptJson->m["Request"] = new Json;
          ptJson->m["Request"]->insert("Gateway", (*connIter)->strName);
          ptJson->m["Request"]->insert("Acorn", strAcorn);
          (*i)->strBuffer[1].append(ptJson->json(strJson)+"\n");
          delete ptJson;
        }
      }
      ssMessage.str("");
      ssMessage << strPrefix << "[" << (*connIter)->strName << "]:  Deregistered gateway.";
      gpCentral->log(ssMessage.str(), strError);
      delete (*connIter);
      gConns.erase(connIter);
    }
  }
  else
  {
    strError = "Please provide the conn iterator.";
    ssMessage.str("");
    ssMessage << strPrefix << " error:  " << strError;
    gpCentral->log(ssMessage.str(), strError);
  }

  return bResult;
}
// }}}
// {{{ acornRegister()
bool acornRegister(string strPrefix, const string strAcorn, const string strName, const string strServer, const string strPort, string &strError)
{
  bool bResult = false;
  list<conn *>::iterator connIter = gConns.end();
  string strJson;
  stringstream ssMessage;
  Json *ptJson;

  strPrefix += "->acornRegister()";
  for (list<conn *>::iterator i = gConns.begin(); connIter == gConns.end() && i != gConns.end(); i++)
  {
    if ((*i)->strName == strName)
    {
      connIter = i;
    }
  }
  if (connIter != gConns.end())
  {
    if (strAcorn != "gateway")
    {
      bool bFound = false;
      for (list<string>::iterator i = (*connIter)->acorns.begin(); !bFound && i != (*connIter)->acorns.end(); i++)
      {
        if ((*i) == strAcorn)
        {
          bFound = true;
        }
      }
      if (!bFound)
      {
        bResult = true;
        (*connIter)->acorns.push_back(strAcorn);
        (*connIter)->acorns.sort();
        ssMessage.str("");
        ssMessage << strPrefix << " [" << (*connIter)->strName << "," << strAcorn << "]:  Registered.";
        gpCentral->log(ssMessage.str(), strError);
        for (list<conn *>::iterator i = gRouters.begin(); i != gRouters.end(); i++)
        {
          if ((*i)->fdSocket != -1)
          {
            ptJson = new Json;
            ptJson->insert("Password", gstrPassword);
            ptJson->insert("Function", "register");
            ptJson->m["Request"] = new Json;
            ptJson->m["Request"]->insert("Gateway", (*connIter)->strName);
            ptJson->m["Request"]->insert("Acorn", strAcorn);
            (*i)->strBuffer[1].append(ptJson->json(strJson)+"\n");
            delete ptJson;
          }
        }
      }
      else
      {
        strError = "The acorn is already registered.";
        ssMessage.str("");
        ssMessage << strPrefix << " error [" << strName << "," << strAcorn << "]:  " << strError;
        gpCentral->log(ssMessage.str(), strError);
      }
    }
    else
    {
      strError = "The gateway is already registered.";
      ssMessage.str("");
      ssMessage << strPrefix << " error [" << strName << "," << strAcorn << "]:  " << strError;
      gpCentral->log(ssMessage.str(), strError);
    }
  }
  else if (strAcorn == "gateway")
  {
    if (!strName.empty())
    {
      if (!strServer.empty())
      {
        if (!strPort.empty())
        {
          conn *ptConn = new conn;
          bResult = true;
          ptConn->fdConnecting = -1;
          ptConn->fdSocket = -1;
          ptConn->strName = strName;
          ptConn->strPort = strPort;
          ptConn->strServer = strServer;
          gConns.push_back(ptConn);
          ssMessage.str("");
          ssMessage << strPrefix << " [" << strName << "]:  Registered gateway.";
          gpCentral->log(ssMessage.str(), strError);
          for (list<conn *>::iterator i = gRouters.begin(); i != gRouters.end(); i++)
          {
            if ((*i)->fdSocket != -1)
            {
              ptJson = new Json;
              ptJson->insert("Password", gstrPassword);
              ptJson->insert("Function", "register");
              ptJson->m["Request"] = new Json;
              ptJson->m["Request"]->insert("Gateway", strName);
              ptJson->m["Request"]->insert("Acorn", strAcorn);
              ptJson->m["Request"]->insert("Server", strServer);
              ptJson->m["Request"]->insert("Port", strPort);
              (*i)->strBuffer[1].append(ptJson->json(strJson)+"\n");
              delete ptJson;
            }
          }
        }
        else
        {
          strError = "Please provide the Port.";
          ssMessage.str("");
          ssMessage << strPrefix << " error [" << strName << "," << strAcorn << "]:  " << strError;
          gpCentral->log(ssMessage.str(), strError);
        }
      }
      else
      {
        strError = "Please provide the Server.";
        ssMessage.str("");
        ssMessage << strPrefix << " error [" << strName << "," << strAcorn << "]:  " << strError;
        gpCentral->log(ssMessage.str(), strError);
      }
    }
    else
    {
      strError = "Please provide the Name.";
      ssMessage.str("");
      ssMessage << strPrefix << " error [" << strName << "," << strAcorn << "]:  " << strError;
      gpCentral->log(ssMessage.str(), strError);
    }
  }
  else
  {
    strError = "The gateway is not registered.";
    ssMessage.str("");
    ssMessage << strPrefix << " error [" << strName << "," << strAcorn << "]:  " << strError;
    gpCentral->log(ssMessage.str(), strError);
  }

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
      else if (strArg == "-h" || strArg == "--help")
      {
        bResult = false;
        mUSAGE(argv[0]);
      }
      else if (strArg == "--syslog")
      {
        gpSyslog = new Syslog(gstrApplication, "router");
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
    gpCentral->setLog(gstrData, "router_", "daily", true, true);
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
// {{{ status()
Json *status()
{
  stringstream ssMessage;
  Json *ptJson, *ptStatus = new Json;

  ptStatus->insert("Name", gstrName);
  ptStatus->m["Buffers"] = new Json;
  ssMessage.str("");
  ssMessage << gstrBuffer[0].size();
  ptStatus->m["Buffers"]->insert("Input", ssMessage.str(), 'n');
  ssMessage.str("");
  ssMessage << gstrBuffer[1].size();
  ptStatus->m["Buffers"]->insert("Output", ssMessage.str(), 'n');
  if (!gConns.empty())
  {
    ptStatus->m["Gateways"] = new Json;
    for (list<conn *>::iterator i = gConns.begin(); i != gConns.end(); i++)
    {
      ptJson = new Json;
      ptJson->insert("Name", (*i)->strName);
      ptJson->insert("Acorns", (*i)->acorns);
      ptJson->m["Buffers"] = new Json;
      ssMessage.str("");
      ssMessage << (*i)->strBuffer[0].size();
      ptJson->m["Buffers"]->insert("Input", ssMessage.str(), 'n');
      ssMessage.str("");
      ssMessage << (*i)->strBuffer[1].size();
      ptJson->m["Buffers"]->insert("Output", ssMessage.str(), 'n');
      ptJson->insert("Connected", (((*i)->fdSocket != -1)?"1":"0"), (((*i)->fdSocket != -1)?'1':'0'));
      ptJson->insert("Enabled", (((*i)->bEnabled)?"1":"0"), (((*i)->bEnabled)?'1':'0'));
      ptJson->insert("Port", (*i)->strPort, 'n');
      ptJson->insert("Server", (*i)->strServer);
      ptStatus->m["Gateways"]->push_back(ptJson);
      delete ptJson;
    }
  }

  return ptStatus;
}
// }}}
