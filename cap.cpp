// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : cap.cpp
// author     : Ben Kietzman
// begin      : 2018-12-17
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

/*! \file cap.cpp
* \brief Acorn Cap
*
* Provides the internal or external cap for an acorn node.
*/
// {{{ includes
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <deque>
#include <iostream>
#include <list>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <thread>
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
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -c CUP, --cup=CUP" << endl << "     Provides the executable command and arguments for the cup." << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -g GATEWAY, --gateway=GATEWAY" << endl << "     Used by an internal type to provide the unix socket path to the gateway." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -m RESIDENT, --memory=RESIDENT" << endl << "     Provides the maximum resident memory size restriction in MB." << endl << endl << " -n NAME, --name=NAME" << endl << "     Provides the acorn, gateway, or router name." << endl << endl << " -p PORT, --port=PORT" << endl << "     Used by an external type to provide the listening port number." << endl << endl << "     --syslog" << endl << "     Enables syslog." << endl << endl << " -t TYPE, --type=TYPE" << endl << "     Provides a cap type of either external or internal." << endl << endl
#define PARENT_READ  readpipe[0]
#define CHILD_WRITE  readpipe[1]
#define CHILD_READ   writepipe[0]
#define PARENT_WRITE writepipe[1]
// }}}
// {{{ structs
struct conn
{
  int fdSocket;
  size_t unUnique;
  string strBuffer[2];
  stringstream ssUnique;
};
// }}}
// {{{ global variables
bool gbShutdown = false; //!< Global shutdown variable.
pid_t gExecPid = -1; //!< Global execute PID.
string gstrApplication = "Acorn"; //!< Global application name.
string gstrCup; //!< Global cup.
string gstrData = "/data/acorn"; //!< Global data directory.
string gstrGateway = "/tmp/acorn_gw"; //!< Global unix socket path for the gateway.
string gstrName; //!< Global acorn or gateway name.
string gstrPort = "22676"; //!< Global listening port number.
string gstrType = "internal"; //!< Global cap type.
unsigned long gulMaxResident = 40 * 1024; //!< Global resident memory restriction in KB.
Central *gpCentral = NULL; //!< Contains the Central class.
Syslog *gpSyslog = NULL; //!< Contains the Syslog class.
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
/*! \fn void monitor(string strPrefix, const string strSection)
* \brief Monitors the health of the running process.
* \param strPrefix Contains the function prefix.
* \param strSection Contains the acorn section.
*/
void monitor(string strPrefix, const string strSection);
/*! \fn void sighandle(const int nSignal)
* \brief Establishes signal handling for the application.
* \param nSignal Contains the caught signal.
*/
void sighandle(const int nSignal);
// }}}
// {{{ main()
int main(int argc, char *argv[], char *env[])
{
  string strError, strPrefix = "main()";

  if (initialize(strPrefix, argc, argv, strError))
  {
    if (!gstrName.empty() && !gstrCup.empty() && ((gstrType == "external" && !gstrPort.empty()) || (gstrType == "internal" && !gstrGateway.empty())))
    {
      char *args[100], *pszArgument;
      int readpipe[2] = {-1, -1}, writepipe[2] = {-1, -1};
      size_t unIndex = 0;
      string strArgument;
      stringstream ssCommand;
      ssCommand.str(gstrCup);
      while (ssCommand >> strArgument)
      {
        pszArgument = new char[strArgument.size() + 1];
        strcpy(pszArgument, strArgument.c_str());
        args[unIndex++] = pszArgument;
      }
      args[unIndex] = NULL;
      if (pipe(readpipe) == 0)
      {
        if (pipe(writepipe) == 0)
        {
          // {{{ child - launch cup
          if ((gExecPid = fork()) == 0)
          {
            close(PARENT_READ);
            close(PARENT_WRITE);
            dup2(CHILD_READ, 0);
            close(CHILD_READ);
            dup2(CHILD_WRITE, 1);
            close(CHILD_WRITE);
            if (gpSyslog != NULL)
            {
              gpSyslog->commandLaunched(gstrCup);
            }
            execve(args[0], args, env);
            cerr << strPrefix << "->execve(" << errno << ") error:  " << strerror(errno) << endl;
            _exit(1);
          }
          // }}}
          // {{{ parent - communication
          else if (gExecPid > 0)
          {
            addrinfo *loggerResult;
            bool bClose = false, bCloseLogger = false, bExit = false, bExternal = ((gstrType == "external")?true:false), bNotifyConnect = false, bRegistered = false;
            char szBuffer[65536];
            int fdLogger = -1, fdLoggerConnecting = -1, fdSocket = -1, nReturn;
            list<conn *> conns;
            list<int> removals;
            list<map<string, string> > logger;
            long lParentArg, lLoggerArg;
            pollfd *fds;
            size_t unIndex, unPosition, unUnique = 0;
            sockaddr *loggerAddr;
            socklen_t loggerAddrlen;
            string strCupBuffer[2], strGatewayBuffer[2], strLoggerBuffer[2], strJson, strLoggerPassword, strLoggerPort = "5648", strLoggerServer, strLoggerUser;
            stringstream ssKey;
            Json *ptJson;
            close(CHILD_READ);
            close(CHILD_WRITE);
            ssKey << "_AcornKey_" << gstrName << "_" << getpid();
            thread threadMonitor(monitor, strPrefix, "cap");
            pthread_setname_np(threadMonitor.native_handle(), "monitor");
            if (!bExternal)
            {
              ptJson = new Json;
              ptJson->insert("_AcornFunction", "register");
              ptJson->insert("_AcornName", gstrName);
              strGatewayBuffer[1].append(ptJson->json(strJson)+"\n");
              delete ptJson;
            }
            if ((lParentArg = fcntl(PARENT_READ, F_GETFL, NULL)) >= 0)
            {
              lParentArg |= O_NONBLOCK;
              fcntl(PARENT_READ, F_SETFL, lParentArg);
            }
            if ((lParentArg = fcntl(PARENT_WRITE, F_GETFL, NULL)) >= 0)
            {
              lParentArg |= O_NONBLOCK;
              fcntl(PARENT_WRITE, F_SETFL, lParentArg);
            }
            while (!gbShutdown && !bExit)
            {
              // {{{ upstream socket
              if (fdSocket == -1)
              {
                // {{{ external
                if (bExternal)
                {
                  addrinfo hints, *result;
                  bool bBound[4] = {false, false, false, false};
                  memset(&hints, 0, sizeof(addrinfo));
                  hints.ai_family = AF_INET6;
                  hints.ai_socktype = SOCK_STREAM;
                  hints.ai_flags = AI_PASSIVE;
                  if ((nReturn = getaddrinfo(NULL, gstrPort.c_str(), &hints, &result)) == 0)
                  {
                    addrinfo *rp;
                    bBound[0] = true;
                    for (rp = result; !bBound[2] && rp != NULL; rp = rp->ai_next)
                    {
                      bBound[1] = bBound[2] = false;
                      if ((fdSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
                      {
                        int nOn = 1;
                        bBound[1] = true;
                        setsockopt(fdSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&nOn, sizeof(nOn));
                        if (bind(fdSocket, rp->ai_addr, rp->ai_addrlen) == 0)
                        {
                          bBound[2] = true;
                          if (listen(fdSocket, SOMAXCONN) == 0)
                          {
                            bBound[3] = true;
                          }
                          else
                          {
                            close(fdSocket);
                            fdSocket = -1;
                          }
                        }
                        else
                        {
                          close(fdSocket);
                          fdSocket = -1;
                        }
                      }
                    }
                    freeaddrinfo(result);
                  }
                  if (!bBound[3])
                  {
                    bExit = true;
                    cerr << strPrefix << "->";
                    if (!bBound[0])
                    {
                      cerr << "getaddrinfo(" << nReturn << ") error:  " << gai_strerror(nReturn);
                    }
                    else
                    {
                      cerr << ((!bBound[1])?"socket":((!bBound[2])?"bind":"listen")) << "(" << errno << ") error:  " << strerror(errno);
                    }
                    cerr << endl;
                  }
                }
                // }}}
                // {{{ internal
                else
                {
                  struct stat tStat;
                  if (stat(gstrGateway.c_str(), &tStat) == 0)
                  {
                    if ((fdSocket = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
                    {
                      sockaddr_un addr;
                      memset(&addr, 0, sizeof(sockaddr_un));
                      addr.sun_family = AF_UNIX;
                      strncpy(addr.sun_path, gstrGateway.c_str(), sizeof(addr.sun_path) - 1);
                      if (connect(fdSocket, (sockaddr *)&addr, sizeof(sockaddr)) == 0)
                      {
                        bNotifyConnect = false;
                      }
                      else
                      {
                        close(fdSocket);
                        fdSocket = -1;
                        if (!bNotifyConnect)
                        {
                          bNotifyConnect = true;
                          cerr << strPrefix << "->socket(" << errno << ") error [" << gstrGateway << "]:  " << strerror(errno) << endl;
                        }
                      }
                    }
                    else if (!bNotifyConnect)
                    {
                      bNotifyConnect = true;
                      cerr << strPrefix << "->socket(" << errno << ") error [" << gstrGateway << "]:  " << strerror(errno) << endl;
                    }
                  }
                  else if (!bNotifyConnect)
                  {
                    bNotifyConnect = true;
                    cerr << strPrefix << "->stat(" << errno << ") error [" << gstrGateway << "]:  " << strerror(errno) << endl;
                  }
                }
                // }}}
              }
              // }}}
              // {{{ logger socket
              if (fdLogger == -1)
              {
                if (fdLoggerConnecting != -1)
                {
                  if (connect(fdLoggerConnecting, loggerAddr, loggerAddrlen) == 0)
                  {
                    cout << strPrefix << "->connect() [logger]:  Connected." << endl;
                    fdLogger = fdLoggerConnecting;
                    fdLoggerConnecting = -1;
                    if (lLoggerArg >= 0)
                    {
                      fcntl(fdLogger, F_SETFL, lLoggerArg);
                    }
                  }
                  else if (errno != EALREADY && errno != EINPROGRESS)
                  {
                    cerr << strPrefix << "->connect(" << errno << ") error [logger]:  " << strerror(errno) << endl;
                    close(fdLoggerConnecting);
                    fdLoggerConnecting = -1;
                    freeaddrinfo(loggerResult);
                  }
                }
                else
                {
                  ifstream inLogger;
                  stringstream ssLogger;
                  ssLogger << gstrData << "/.cred/logger";
                  inLogger.open(ssLogger.str().c_str());
                  if (inLogger)
                  {
                    if (getline(inLogger, strJson))
                    {
                      ptJson = new Json(strJson);
                      if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty() && ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty() && ptJson->m.find("Server") != ptJson->m.end() && !ptJson->m["Server"]->v.empty())
                      {
                        addrinfo hints;
                        bool bConnected[2] = {false, false};
                        strLoggerUser = ptJson->m["User"]->v;
                        strLoggerPassword = ptJson->m["Password"]->v;
                        strLoggerServer = ptJson->m["Server"]->v;
                        memset(&hints, 0, sizeof(addrinfo));
                        hints.ai_family = AF_UNSPEC;
                        hints.ai_socktype = SOCK_STREAM;
                        if ((nReturn = getaddrinfo(strLoggerServer.c_str(), strLoggerPort.c_str(), &hints, &(loggerResult))) == 0)
                        {
                          addrinfo *rp;
                          for (rp = (loggerResult); !bConnected[1] && rp != NULL; rp = rp->ai_next)
                          {
                            bConnected[0] = false;
                            if ((fdLoggerConnecting = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
                            {
                              long lArg;
                              loggerAddr = rp->ai_addr;
                              loggerAddrlen = rp->ai_addrlen;
                              if ((lLoggerArg = lArg = fcntl(fdLoggerConnecting, F_GETFL, NULL)) >= 0)
                              {
                                lArg |= O_NONBLOCK;
                                fcntl(fdLoggerConnecting, F_SETFL, lArg);
                              }
                              if (connect(fdLoggerConnecting, loggerAddr, loggerAddrlen) == 0)
                              {
                                cout << strPrefix << "->connect() [logger]:  Connected." << endl;
                                fdLogger = fdLoggerConnecting;
                                fdLoggerConnecting = -1;
                                if (lLoggerArg >= 0)
                                {
                                  fcntl(fdLogger, F_SETFL, lLoggerArg);
                                }
                              }
                              else if (errno != EALREADY && errno != EINPROGRESS)
                              {
                                cerr << strPrefix << "->connect(" << errno << ") error [logger]:  " << strerror(errno) << endl;
                                close(fdLoggerConnecting);
                                fdLoggerConnecting = -1;
                                freeaddrinfo(loggerResult);
                              }
                            }
                            else
                            {
                              cerr << strPrefix << "->socket(" << errno << ") error [logger]:  " << strerror(errno) << endl;
                              freeaddrinfo(loggerResult);
                            }
                          }
                        }
                        else
                        {
                          cerr << strPrefix << "->getaddrinfo(" << nReturn << ") error [logger]:  " << gai_strerror(nReturn) << endl;
                        }
                      }
                      else
                      {
                        cerr << strPrefix << " error [logger]:  Invalide credentials." << endl;
                      }
                      delete ptJson;
                    }
                    else
                    {
                      cerr << strPrefix << "->getline(" << errno << ") error [logger]:  " << strerror(errno) << endl;
                    }
                  }
                  inLogger.clear();
                }
              }
              // }}}
              // {{{ prep sockets
              unIndex = 1;
              if (!strCupBuffer[1].empty())
              {
                unIndex++;
              }
              if (fdSocket != -1)
              {
                unIndex++;
              }
              if (fdLogger != -1)
              {
                unIndex++;
              }
              if (bExternal)
              {
                unIndex += conns.size();
              }
              fds = new pollfd[unIndex];
              unIndex = 0;
              fds[unIndex].fd = PARENT_READ;
              fds[unIndex].events = POLLIN;
              unIndex++;
              if (!strCupBuffer[1].empty())
              {
                fds[unIndex].fd = PARENT_WRITE;
                fds[unIndex].events = POLLOUT;
                unIndex++;
              }
              if (fdSocket != -1)
              {
                fds[unIndex].fd = fdSocket;
                fds[unIndex].events = POLLIN;
                if (!bExternal && !strGatewayBuffer[1].empty())
                {
                  fds[unIndex].events |= POLLOUT;
                }
                unIndex++;
              }
              if (fdLogger != -1)
              {
                fds[unIndex].fd = fdLogger;
                fds[unIndex].events = POLLIN;
                while (!logger.empty())
                {
                  map<string, string> label = logger.front();
                  string strMessage = ".";
                  logger.front().clear();
                  logger.pop_front();
                  if (label.find("loggerMessage") != label.end())
                  {
                    if (!label["loggerMessage"].empty())
                    {
                      strMessage = label["loggerMessage"];
                    }
                    label.erase("loggerMessage");
                  }
                  ptJson = new Json;
                  ptJson->insert("Application", gstrApplication);
                  ptJson->insert("User", strLoggerUser);
                  ptJson->insert("Password", strLoggerPassword);
                  ptJson->insert("Function", "message");
                  ptJson->insert("Message", strMessage);
                  ptJson->m["Label"] = new Json(label);
                  label.clear();
                  strLoggerBuffer[1].append(ptJson->json(strJson)+"\n");
                  delete ptJson;
                }
                if (!strLoggerBuffer[1].empty())
                {
                  fds[unIndex].events |= POLLOUT;
                }
                unIndex++;
              }
              if (bExternal)
              {
                for (list<conn *>::iterator i = conns.begin(); i != conns.end(); i++)
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
                  // {{{ read from cup
                  if (fds[i].fd == PARENT_READ)
                  {
                    if (fds[i].revents & (POLLHUP | POLLIN))
                    {
                      if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                      {
                        strCupBuffer[0].append(szBuffer, nReturn);
                        while ((unPosition = strCupBuffer[0].find("\n")) != string::npos)
                        {
                          Json *ptResponse = new Json(strCupBuffer[0].substr(0, unPosition));
                          if (ptResponse->m.find("acornDebug") != ptResponse->m.end())
                          {
                            bool bDebugFound = false;
                            timespec stop;
                            clock_gettime(CLOCK_REALTIME, &stop);
                            for (list<Json *>::iterator j = ptResponse->m["acornDebug"]->l.begin(); !bDebugFound && j != ptResponse->m["acornDebug"]->l.end(); j++)
                            {
                              if ((*j)->m.find("Cup") != (*j)->m.end() && (*j)->m["Cup"]->v == gstrCup)
                              {
                                bDebugFound = true;
                                if ((*j)->m.find("Timing") != (*j)->m.end())
                                {
                                  stringstream ssNanoseconds, ssSeconds;
                                  ssSeconds << stop.tv_sec;
                                  ssNanoseconds << stop.tv_nsec;
                                  if ((*j)->m["Timing"]->m.find("Stop") == (*j)->m["Timing"]->m.end())
                                  {
                                    (*j)->m["Timing"]->m["Stop"] = new Json;
                                  }
                                  (*j)->m["Timing"]->m["Stop"]->insert("Seconds", ssSeconds.str(), 'n');
                                  (*j)->m["Timing"]->m["Stop"]->insert("Nanoseconds", ssNanoseconds.str(), 'n');
                                  if ((*j)->m["Timing"]->m.find("Start") != (*j)->m["Timing"]->m.end() && (*j)->m["Timing"]->m["Start"]->m.find("Seconds") != (*j)->m["Timing"]->m["Start"]->m.end() && !(*j)->m["Timing"]->m["Start"]->m["Seconds"]->v.empty() && (*j)->m["Timing"]->m["Start"]->m.find("Nanoseconds") != (*j)->m["Timing"]->m["Start"]->m.end() && !(*j)->m["Timing"]->m["Start"]->m["Nanoseconds"]->v.empty())
                                  {
                                    size_t unDuration = 0;
                                    stringstream ssDuration, ssSubNanoseconds((*j)->m["Timing"]->m["Start"]->m["Nanoseconds"]->v), ssSubSeconds((*j)->m["Timing"]->m["Start"]->m["Seconds"]->v);
                                    timespec start;
                                    ssSubSeconds >> start.tv_sec;
                                    ssSubNanoseconds >> start.tv_nsec;
                                    unDuration = ((stop.tv_sec - start.tv_sec) * 1000) + ((stop.tv_nsec - start.tv_nsec) / 1000000);
                                    ssDuration << unDuration;
                                    (*j)->m["Timing"]->insert("Duration", ssDuration.str(), 'n');
                                  }
                                }
                              }
                            }
                          }
                          // {{{ external
                          if (bExternal)
                          {
                            if (ptResponse->m.find(ssKey.str()) != ptResponse->m.end() && !ptResponse->m[ssKey.str()]->v.empty())
                            {
                              bool bFound = false;
                              string strUnique = ptResponse->m[ssKey.str()]->v;
                              delete ptResponse->m[ssKey.str()];
                              ptResponse->m.erase(ssKey.str());
                              for (list<conn *>::iterator j = conns.begin(); !bFound && j != conns.end(); j++)
                              {
                                if ((*j)->ssUnique.str() == strUnique)
                                {
                                  bFound = true;
                                  (*j)->strBuffer[1].append(ptResponse->json(strJson)+"\n");
                                }
                              }
                            }
                          }
                          // }}}
                          // {{{ internal
                          else
                          {
                            strGatewayBuffer[1].append(ptResponse->json(strJson)+"\n");
                          }
                          // }}}
                          delete ptResponse;
                          strCupBuffer[0].erase(0, (unPosition + 1));
                        }
                      }
                      else
                      {
                        bExit = true;
                        if (nReturn < 0)
                        {
                          cerr << strPrefix << "->read(" << errno << ") error [cup]:  " << strerror(errno) << endl;
                        }
                      }
                    }
                  }
                  // }}}
                  // {{{ write to cup
                  else if (fds[i].fd == PARENT_WRITE)
                  {
                    if (fds[i].revents & POLLOUT)
                    {
                      if ((nReturn = write(fds[i].fd, strCupBuffer[1].c_str(), strCupBuffer[1].size())) > 0)
                      {
                        strCupBuffer[1].erase(0, nReturn);
                      }
                      else
                      {
                        bExit = true;
                        if (nReturn < 0)
                        {
                          cerr << strPrefix << "->write(" << errno << ") error [cup]:  " << strerror(errno) << endl;
                        }
                      }
                    }
                  }
                  // }}}
                  // {{{ upstream communication
                  else if (fds[i].fd == fdSocket)
                  {
                    // {{{ external
                    if (bExternal)
                    {
                      // {{{ accept incoming connection
                      if (fds[i].revents & POLLIN)
                      {
                        int fdClient;
                        socklen_t clilen;
                        sockaddr_in cli_addr;
                        clilen = sizeof(cli_addr);
                        if ((fdClient = accept(fds[i].fd, (sockaddr *)&cli_addr, &clilen)) >= 0)
                        {
                          bool bFound = true;
                          conn *ptConn = new conn;
                          if (gpSyslog != NULL)
                          { 
                            gpSyslog->connectionStarted("Accepted an incoming request.", fdClient);
                          }
                          ptConn->fdSocket = fdClient;
                          while (bFound)
                          {
                            bFound = false;
                            for (list<conn *>::iterator j = conns.begin(); !bFound && j != conns.end(); j++)
                            {
                              if ((*j)->unUnique == unUnique)
                              {
                                bFound = true;
                                unUnique++;
                              }
                            }
                          }
                          ptConn->unUnique = unUnique++;
                          ptConn->ssUnique << ptConn->unUnique;
                          conns.push_back(ptConn);
                        }
                        else
                        {
                          cerr << strPrefix << "->accept(" << errno << ") error:  " << strerror(errno) << endl;
                        }
                      }
                      // }}}
                    }
                    // }}}
                    // {{{ internal
                    else
                    {
                      // {{{ read from gateway
                      if (fds[i].revents & POLLIN)
                      {
                        if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                        {
                          strGatewayBuffer[0].append(szBuffer, nReturn);
                          while ((unPosition = strGatewayBuffer[0].find("\n")) != string::npos)
                          {
                            map<string, string> label;
                            ptJson = new Json(strGatewayBuffer[0].substr(0, unPosition));
                            if (ptJson->m.find("acornDebug") != ptJson->m.end())
                            {
                              stringstream ssNanoseconds, ssSeconds;
                              timespec start;
                              Json *ptSubJson = new Json;
                              clock_gettime(CLOCK_REALTIME, &start);
                              ptSubJson->insert("Cup", gstrCup);
                              ptSubJson->insert("Name", gstrName);
                              ptSubJson->m["Timing"] = new Json;
                              ssSeconds << start.tv_sec;
                              ssNanoseconds << start.tv_nsec;
                              ptSubJson->m["Timing"]->m["Start"] = new Json;
                              ptSubJson->m["Timing"]->m["Start"]->insert("Seconds", ssSeconds.str(), 'n');
                              ptSubJson->m["Timing"]->m["Start"]->insert("Nanoseconds", ssNanoseconds.str(), 'n');
                              ptJson->m["acornDebug"]->push_back(ptSubJson);
                              delete ptSubJson;
                            }
                            if (bRegistered)
                            {
                              strCupBuffer[1].append(ptJson->json(strJson)+"\n");
                            }
                            else if (ptJson->m.find("_AcornFunction") != ptJson->m.end() && ptJson->m["_AcornFunction"]->v == "register")
                            {
                              if (ptJson->m.find("Status") != ptJson->m.end() && ptJson->m["Status"]->v == "okay")
                              {
                                bRegistered = true;
                              }
                              else if (ptJson->m.find("Error") != ptJson->m.end() && !ptJson->m["Error"]->v.empty())
                              {
                                cerr << strPrefix << " error:  " << ptJson->m["Error"]->v << endl;
                              }
                              else
                              {
                                cerr << strPrefix << " error:  Encountered an unknown error." << endl;
                              }
                              if (!bRegistered)
                              {
                                Json *ptSubJson = new Json;
                                ptSubJson->insert("_AcornFunction", "register");
                                ptSubJson->insert("_AcornName", gstrName);
                                strGatewayBuffer[1].append(ptSubJson->json(strJson)+"\n");
                                delete ptSubJson;
                              }
                            }
                            if (ptJson->m.find("Password") != ptJson->m.end())
                            {
                              delete ptJson->m["Password"];
                              ptJson->m.erase("Password");
                            }
                            label["loggerMessage"] = ptJson->json(strJson);
                            label["Type"] = ((bExternal)?"external":"internal");
                            if (bExternal)
                            {
                              label["Name"] = gstrName;
                            }
                            else
                            {
                              label["Acorn"] = gstrName;
                            }
                            label["Cup"] = gstrCup;
                            logger.push_back(label);
                            label.clear();
                            delete ptJson;
                            strGatewayBuffer[0].erase(0, (unPosition + 1));
                          }
                        }
                        else
                        {
                          bClose = true;
                          bRegistered = false;
                          ptJson = new Json;
                          ptJson->insert("_AcornFunction", "register");
                          ptJson->insert("_AcornName", gstrName);
                          strGatewayBuffer[1].append(ptJson->json(strJson)+"\n");
                          delete ptJson;
                          if (nReturn < 0)
                          {
                            cerr << strPrefix << "->read(" << errno << ") error [gateway]:  " << strerror(errno) << endl;
                          }
                        }
                      }
                      // }}}
                      // {{{ write to gateway
                      if (fds[i].revents & POLLOUT)
                      {
                        if ((nReturn = write(fds[i].fd, strGatewayBuffer[1].c_str(), strGatewayBuffer[1].size())) > 0)
                        {
                          strGatewayBuffer[1].erase(0, nReturn);
                        }
                        else
                        {
                          bClose = true;
                          bRegistered = false;
                          ptJson = new Json;
                          ptJson->insert("_AcornFunction", "register");
                          ptJson->insert("_AcornName", gstrName);
                          strGatewayBuffer[1].append(ptJson->json(strJson)+"\n");
                          delete ptJson;
                          if (nReturn < 0)
                          {
                            cerr << strPrefix << "->write(" << errno << ") error [gateway]:  " << strerror(errno) << endl;
                          }
                        }
                      }
                      // }}}
                    }
                    // }}}
                  }
                  // }}}
                  // {{{ logger
                  else if (fds[i].fd == fdLogger)
                  {
                    // {{{ read from logger
                    if (fds[i].revents & POLLIN)
                    {
                      if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                      {
                        strLoggerBuffer[0].append(szBuffer, nReturn);
                        while ((unPosition = strLoggerBuffer[0].find("\n")) != string::npos)
                        {
                          ptJson = new Json(strLoggerBuffer[0].substr(0, unPosition));
                          strLoggerBuffer[0].erase(0, (unPosition + 1));
                          if (ptJson->m.find("Status") == ptJson->m.end() || ptJson->m["Status"]->v != "okay")
                          {
                            if (ptJson->m.find("Error") != ptJson->m.end() && !ptJson->m["Error"]->v.empty())
                            {
                              cerr << strPrefix << " error [logger]:  " << ptJson->m["Error"]->v << endl;
                            }
                            else
                            {
                              cerr << strPrefix << " error [logger]:  Encountered an unknown error." << endl;
                            }
                          }
                          delete ptJson;
                        }
                      }
                      else
                      {
                        bCloseLogger = true;
                        if (nReturn < 0)
                        {
                          cerr << strPrefix << "->read(" << errno << ") error [logger]:  " << strerror(errno) << endl;
                        }
                      }
                    }
                    // }}}
                    // {{{ write to logger
                    else if (fds[i].revents & POLLOUT)
                    {
                      if ((nReturn = write(fds[i].fd, strLoggerBuffer[1].c_str(), strLoggerBuffer[1].size())) > 0)
                      {
                        strLoggerBuffer[1].erase(0, nReturn);
                      }
                      else
                      {
                        bCloseLogger = true;
                        if (nReturn < 0)
                        {
                          cerr << strPrefix << "->write(" << errno << ") error [logger]:  " << strerror(errno) << endl;
                        }
                      }
                    }
                    // }}}
                  }
                  // }}}
                  // {{{ upstream clients
                  else if (bExternal)
                  {
                    bool bFound = false;
                    for (list<conn *>::iterator j = conns.begin(); !bFound && j != conns.end(); j++)
                    {
                      if (fds[i].fd == (*j)->fdSocket)
                      {
                        bFound = true;
                        // {{{ read from client
                        if (fds[i].revents & POLLIN)
                        {
                          if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                          {
                            (*j)->strBuffer[0].append(szBuffer, nReturn);
                            while ((unPosition = (*j)->strBuffer[0].find("\n")) != string::npos)
                            {
                              map<string, string> label;
                              Json *ptRequest = new Json((*j)->strBuffer[0].substr(0, unPosition));
                              (*j)->strBuffer[0].erase(0, (unPosition + 1));
                              ptRequest->insert(ssKey.str(), (*j)->ssUnique.str(), 'n');
                              if (ptRequest->m.find("acornDebug") != ptRequest->m.end())
                              {
                                stringstream ssNanoseconds, ssSeconds;
                                timespec start;
                                Json *ptJson = new Json;
                                clock_gettime(CLOCK_REALTIME, &start);
                                ptJson->insert("Cup", gstrCup);
                                ptJson->insert("Name", gstrName);
                                ptJson->m["Timing"] = new Json;
                                ssSeconds << start.tv_sec;
                                ssNanoseconds << start.tv_nsec;
                                ptJson->m["Timing"]->m["Start"] = new Json;
                                ptJson->m["Timing"]->m["Start"]->insert("Seconds", ssSeconds.str(), 'n');
                                ptJson->m["Timing"]->m["Start"]->insert("Nanoseconds", ssNanoseconds.str(), 'n');
                                ptRequest->m["acornDebug"]->push_back(ptJson);
                                delete ptJson;
                              }
                              strCupBuffer[1].append(ptRequest->json(strJson)+"\n");
                              if (ptRequest->m.find("Password") != ptRequest->m.end())
                              {
                                delete ptRequest->m["Password"];
                                ptRequest->m.erase("Password");
                              }
                              label["loggerMessage"] = ptRequest->json(strJson);
                              label["Type"] = ((bExternal)?"external":"internal");
                              if (bExternal)
                              {
                                label["Name"] = gstrName;
                              }
                              else
                              {
                                label["Acorn"] = gstrName;
                              }
                              label["Cup"] = gstrCup;
                              logger.push_back(label);
                              label.clear();
                              delete ptRequest;
                            }
                          }
                          else
                          {
                            removals.push_back((*j)->fdSocket);
                            if (nReturn < 0)
                            {
                              cerr << strPrefix << "->read(" << errno << ") error [client]:  " << strerror(errno) << endl;
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
                            removals.push_back((*j)->fdSocket);
                            if (nReturn < 0)
                            {
                              cerr << strPrefix << "->write(" << errno << ") error [client]:  " << strerror(errno) << endl;
                            }
                          }
                        }
                        // }}}
                      }
                    }
                  }
                  // }}}
                }
              }
              else if (nReturn < 0)
              {
                bExit = true;
                cerr << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno) << endl;
              }
              delete[] fds;
              if (bClose)
              {
                bClose = false;
                close(fdSocket);
                fdSocket = -1;
              }
              if (bCloseLogger)
              {
                bCloseLogger = false;
                close(fdLogger);
                fdLogger = -1;
              }
              for (list<int>::iterator i = removals.begin(); i != removals.end(); i++)
              {
                list<conn *>::iterator connsIter = conns.end();
                for (list<conn *>::iterator j = conns.begin(); connsIter == conns.end() && j != conns.end(); j++)
                {
                  if ((*i) == (*j)->fdSocket)
                  {
                    connsIter = j;
                  }
                }
                if (connsIter != conns.end())
                {
                  if (gpSyslog != NULL)
                  { 
                    gpSyslog->connectionStopped("Closed request.", (*connsIter)->fdSocket);
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
              if (gpSyslog != NULL)
              { 
                gpSyslog->connectionStopped("Closed request.", conns.front()->fdSocket);
              }
              close(conns.front()->fdSocket);
              delete conns.front();
              conns.pop_front();
            }
            if (fdSocket != -1)
            {
              close(fdSocket);
            }
            close(PARENT_READ);
            close(PARENT_WRITE);
            if (gpSyslog != NULL)
            {
              gpSyslog->commandEnded(gstrCup);
            }
            gbShutdown = true;
            threadMonitor.join();
            while (!logger.empty())
            {
              logger.front().clear();
              logger.pop_front();
            }
            kill(gExecPid, SIGTERM);
          }
          // }}}
          // {{{ error
          else
          {
            cerr << strPrefix << "->fork(" << errno << ") error:  " << strerror(errno) << endl;
          }
          // }}}
          gExecPid = -1;
        }
        else
        {
          cerr << strPrefix << "->pipe(" << errno << ") error [write]:  " << strerror(errno) << endl;
        }
      }
      else
      {
        cerr << strPrefix << "->pipe(" << errno << ") error [read]:  " << strerror(errno) << endl;
      }
    }
    else
    {
      mUSAGE(argv[0]);
    }
    if (gpSyslog != NULL)
    {
      delete gpSyslog;
    }
    delete gpCentral;
  }
  else
  {
    cerr << strPrefix << "->initialize() error:  " << strError << endl;
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
      if (strArg == "-c" || (strArg.size() > 6 && strArg.substr(0, 6) == "--cup="))
      {
        if (strArg == "-c" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrCup = argv[++i];
        }
        else
        {
          gstrCup = strArg.substr(6, strArg.size() - 6);
        }
        gpCentral->manip()->purgeChar(gstrCup, gstrCup, "'");
        gpCentral->manip()->purgeChar(gstrCup, gstrCup, "\"");
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
      else if (strArg == "-g" || (strArg.size() > 10 && strArg.substr(0, 10) == "--gateway="))
      {
        if (strArg == "-g" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrGateway = argv[++i];
        }
        else
        {
          gstrGateway = strArg.substr(10, strArg.size() - 10);
        }
        gpCentral->manip()->purgeChar(gstrGateway, gstrGateway, "'");
        gpCentral->manip()->purgeChar(gstrGateway, gstrGateway, "\"");
      }
      else if (strArg == "-h" || strArg == "--help")
      {
        bResult = false;
        mUSAGE(argv[0]);
      }
      else if (strArg == "-m" || (strArg.size() > 9 && strArg.substr(0, 9) == "--memory="))
      {
        stringstream ssMaxResident;
        if (strArg == "-m" && i + 1 < argc && argv[i+1][0] != '-')
        {
          ssMaxResident.str(argv[++i]);
        }
        else
        {
          ssMaxResident.str(strArg.substr(9, strArg.size() - 9));
        }
        ssMaxResident >> gulMaxResident;
        gulMaxResident *= 1024;
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
      else if (strArg == "--syslog")
      {
        gpSyslog = new Syslog(gstrApplication, "cap");
      }
      else if (strArg == "-t" || (strArg.size() > 7 && strArg.substr(0, 7) == "--type="))
      {
        if (strArg == "-t" && i + 1 < argc && argv[i+1][0] != '-')
        {
          gstrType = argv[++i];
        }
        else
        {
          gstrType = strArg.substr(7, strArg.size() - 7);
        }
        gpCentral->manip()->purgeChar(gstrType, gstrType, "'");
        gpCentral->manip()->purgeChar(gstrType, gstrType, "\"");
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
  }

  return bResult;
}
// }}}
// {{{ monitor()
void monitor(string strPrefix, const string strSection)
{
  ifstream inStat;
  string strError, strField;
  unsigned long ulResident;

  strPrefix += "->monitor()";
  while (!gbShutdown)
  {
    pid_t nPid[2] = {getpid(), gExecPid};
    for (size_t i = 0; i < 2; i++)
    {
      stringstream ssStat;
      ssStat << "/proc/" << nPid[i] << "/stat";
      inStat.open(ssStat.str().c_str());
      if (inStat)
      {
        for (size_t i = 0; inStat && i < 23; i++)
        {
          inStat >> strField;
        }
        if (inStat)
        {
          if (inStat >> ulResident)
          {
            long lPageSize = sysconf(_SC_PAGE_SIZE) / 1024;
            ulResident *= lPageSize;
            if (ulResident >= gulMaxResident)
            {
              cerr << strPrefix << " error [" << gstrName << "," << strSection << "]:  Restarting the acorn due to the " << ((i == 0)?"cap":"cup") << " having a resident size of " << ulResident << " KB which exceeds the maximum resident restriction of " << gulMaxResident << " KB." << endl;
              gbShutdown = true;
              sighandle(SIGTERM);
            }
          }
          else
          {
            cerr << strPrefix << "->ifstream>>(" << errno << ") error [" << gstrName << "," << strSection << "," << ssStat.str() << "]:  " << strerror(errno) << endl;
            gbShutdown = true;
            sighandle(SIGTERM);
          }
        }
        else
        {
          cerr << strPrefix << "->ifstream>>(" << errno << ") error [" << gstrName << "," << strSection << "," << ssStat.str() << "]:  " << strerror(errno) << endl;
          gbShutdown = true;
          sighandle(SIGTERM);
        }
      }
      else
      {
        cerr << strPrefix << "->ifstream::open(" << errno << ") error [" << gstrName << "," << strSection << "," << ssStat.str() << "]:  " << strerror(errno) << endl;
        gbShutdown = true;
        sighandle(SIGTERM);
      }
      inStat.close();
    }
    for (size_t i = 0; !gbShutdown && i < 240; i++)
    {
      usleep(250000);
    }
  }
}
// }}}
// {{{ sighandle()
void sighandle(const int nSignal)
{
  string strError, strSignal;

  sethandles(sigdummy);
  gbShutdown = true;
  if (nSignal != SIGINT && nSignal != SIGTERM)
  {
    cerr << "sighandle(" << nSignal << ") error:  Caught a " << sigstring(strSignal, nSignal) << " signal." << endl;
  }
  exit(1);
}
// }}}
