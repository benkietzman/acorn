// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : mysql.cpp
// author     : Ben Kietzman
// begin      : 2019-01-09
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

/*! \file mysql.cpp
* \brief Acorn Cup - mysql
*
* Provides the mysql cup for an acorn node.
*/
// {{{ includes
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <poll.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace std;
#include <Central>
#include <Json>
#include <SignalHandling>
using namespace common;
#include "include/Mysql"
using namespace acorn;
// }}}
// {{{ defines
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -c CONNECTIONS, --connections=CONNECTIONS" << endl << "     Provides the maximum number of MySQL socket connections per unique database key." << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -q QUEUE, --queue=QUEUE" << endl << "     Provides the maximum resident maximum number of queued MySQL requests per socket connection before opening a new socket connection." << endl << endl
// }}}
// {{{ global variables
bool gbShutdown = false; //!< Global shutdown variable.
mutex gMutexRequest; //!< Global request mutex.
mutex gMutexOutput; //!< Global output mutex.
size_t gunMaxConnections = 10; //!< Global connections.
size_t gunMaxQueue = 5; //!< Global queue.
size_t gunRequests = 0; //!< Global requests.
string gstrApplication = "Acorn"; //!< Global application name.
string gstrBuffer[2]; //!< Global buffers.
string gstrData = "/data/acorn"; //!< Global data directory.
string gstrName; //!< Global acorn or gateway name.
Central *gpCentral = NULL; //!< Contains the Central class.
Mysql *gpMysql = NULL; //!< Contains the Mysql class.
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
/*! \fn bool mysql(const string strType, const string strUser, const string strPassword, const string strServer, const unsigned int unPort, const string strDatabase, const string strQuery, list<map<string, string> > *rows, unsigned long long &ullID, unsigned long long &ullRows, string &strError)
* \brief Performs thread-safe queries.
* \param strType Contains the database request type.
* \param strUser Contains the database user.
* \param strPassword Contains the database password.
* \param strServer Contains the database server.
* \param unPort Contains the database port.
* \param strDatabase Contains the database name.
* \param strQuery Contains the database query.
* \param rows Contains the database query result.
* \param ullID Contains the database insert ID.
* \param ullRows Contains the database affected rows.
* \param strError Contains the error string.
* \return Returns a boolean true/false value.
*/
bool mysql(string strType, const string strUser, const string strPassword, const string strServer, const unsigned int unPort, const string strDatabase, const string strQuery, list<map<string, string> > *rows, unsigned long long &ullID, unsigned long long &ullRows, string &strError);
/*! \fn void request(Json *ptJson)
* \brief Processes a request
* \param ptJson Contains the request.
*/
void request(Json *ptJson);
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
      string strJson;
      time_t CTime[3];
      time(&(CTime[0]));
      CTime[1] = CTime[0];
      gpMysql = new Mysql(gunMaxConnections, gunMaxQueue);
      while (!gbShutdown)
      {
        unIndex = ((!gstrBuffer[1].empty())?2:1);
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
                  gstrBuffer[0].append(szBuffer, nReturn);
                  while ((unPosition = gstrBuffer[0].find("\n")) != string::npos)
                  {
                    Json *ptJson = new Json(gstrBuffer[0].substr(0, unPosition));
                    gstrBuffer[0].erase(0, (unPosition + 1));
                    gMutexRequest.lock();
                    gunRequests++;
                    gMutexRequest.unlock();
                    thread threadRequest(request, ptJson);
                    pthread_setname_np(threadRequest.native_handle(), "request");
                    threadRequest.detach();
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
                gMutexOutput.lock();
                if ((nReturn = write(fds[i].fd, gstrBuffer[1].c_str(), gstrBuffer[1].size())) > 0)
                {
                  gstrBuffer[1].erase(0, nReturn);
                }
                else
                {
                  gbShutdown = true;
                  ssMessage.str("");
                  ssMessage << strPrefix << "->write(" << errno << ") error:  " << strerror(errno);
                  gpCentral->log(ssMessage.str(), strError);
                }
                gMutexOutput.unlock();
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
        time(&(CTime[2]));
        if ((CTime[2] - CTime[0]) >= 60)
        {
          CTime[0] = CTime[2];
          gpMysql->disconnect();
        }
        if ((CTime[2] - CTime[1]) >= 900)
        {
          map<string, list<acorn_mysql *> > *pConn;
          CTime[1] = CTime[2];
          gpMysql->lock();
          pConn = gpMysql->conn();
          for (auto &i : *pConn)
          {
            vector<string> keys;
            size_t unConnections = i.second.size(), unQueued = 0;
            string strToken;
            stringstream ssName(i.first);
            while (getline(ssName, strToken, '|'))
            {
              keys.push_back(strToken);
            }
            ssMessage.str("");
            ssMessage << strPrefix << " [";
            for (size_t j = 0; j < keys.size(); j++)
            {
              if (j != 1)
              {
                if (j > 0)
                {
                  ssMessage << '|';
                }
                ssMessage << keys[j];
              }
            }
            for (auto &j : i.second)
            {
              unQueued += j->unThreads;
            }
            ssMessage << "]:  STATUS - " << unQueued << " queued across " << unConnections << " connections.";
            gpCentral->log(ssMessage.str(), strError);
            keys.clear();
          }
          gpMysql->unlock();
        }
      }
      gbShutdown = true;
      while (gunRequests > 0)
      {
        usleep(250000);
      }
      delete gpMysql;
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
      if (strArg == "-c" || (strArg.size() > 14 && strArg.substr(0, 14) == "--connections="))
      {
        stringstream ssMaxConnections;
        if (strArg == "-c" && i + 1 < argc && argv[i+1][0] != '-')
        {
          ssMaxConnections.str(argv[++i]);
        }
        else
        {
          ssMaxConnections.str(strArg.substr(14, strArg.size() - 14));
        }
        ssMaxConnections >> gunMaxConnections;
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
      else if (strArg == "-q" || (strArg.size() > 8 && strArg.substr(0, 8) == "--queue="))
      {
        stringstream ssMaxQueue;
        if (strArg == "-q" && i + 1 < argc && argv[i+1][0] != '-')
        {
          ssMaxQueue.str(argv[++i]);
        }
        else
        {
          ssMaxQueue.str(strArg.substr(8, strArg.size() - 8));
        }
        ssMaxQueue >> gunMaxQueue;
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
    gpCentral->setLog(gstrData, "mysql_", "monthly", true, true);
    gpCentral->setRoom("#system");
  }

  return bResult;
}
// }}}
// {{{ mysql()
bool mysql(const string strType, const string strUser, const string strPassword, const string strServer, const unsigned int unPort, const string strDatabase, const string strQuery, list<map<string, string> > *rows, unsigned long long &ullID, unsigned long long &ullRows, string &strError)
{
  bool bResult = false;
  list<acorn_mysql *>::iterator mysqlIter;
  stringstream ssError;

  if (rows != NULL)
  {
    for (auto &i : *rows)
    {
      i.clear();
    }
    rows->clear();
  }
  if (gpMysql->connect(strUser, strPassword, strServer, unPort, strDatabase, mysqlIter, strError))
  {
    if (!strType.empty())
    {
      (*mysqlIter)->secure.lock();
      if (strType == "query")
      {
        if (rows != NULL)
        {
          MYSQL_RES *result = gpMysql->query(mysqlIter, strQuery, ullRows, strError);
          if (result != NULL)
          {
            vector<string> fields;
            if (gpMysql->fields(result, fields))
            {
              map<string, string> *row;
              bResult = true;
              while ((row = gpMysql->fetch(result, fields)) != NULL)
              {
                rows->push_back(*row);
                row->clear();
                delete row;
              }
            }
            else
            {
              strError = "Failed to fetch field names.";
            }
            fields.clear();
            gpMysql->free(result);
          }
        }
        else
        {
          strError = "Please provide a placeholder for the resultant rows.";
        }
      }
      else if (strType == "update")
      {
        bResult = gpMysql->update(mysqlIter, strQuery, ullID, ullRows, strError);
      }
      else
      {
        strError = "Please provide a valid Type:  query, update.";
      }
      (*mysqlIter)->secure.unlock();
    }
    else
    {
      strError = "Please provide the Type.";
    }
    gpMysql->disconnect(mysqlIter);
  }

  return bResult;
}
// }}}
// {{{ request()
void request(Json *ptJson)
{
  bool bProcessed = false;
  string strError, strJson;

  if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
  {
    if (ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty())
    {
      if (ptJson->m.find("Server") != ptJson->m.end() && !ptJson->m["Server"]->v.empty())
      {
        unsigned int unPort = 3306;
        if (ptJson->m.find("Port") != ptJson->m.end() && !ptJson->m["Port"]->v.empty())
        {
          stringstream ssPort(ptJson->m["Port"]->v);
          ssPort >> unPort;
        }
        if (ptJson->m.find("Database") != ptJson->m.end() && !ptJson->m["Database"]->v.empty())
        {
          if (ptJson->m.find("Request") != ptJson->m.end())
          {
            unsigned long long ullID = 0, ullRows = 0;
            if (ptJson->m["Request"]->m.find("Query") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["Query"]->v.empty())
            {
              list<map<string, string> > *result = new list<map<string, string> >;
              if (mysql("query", ptJson->m["User"]->v, ptJson->m["Password"]->v, ptJson->m["Server"]->v, unPort, ptJson->m["Database"]->v, ptJson->m["Request"]->m["Query"]->v, result, ullID, ullRows, strError))
              {
                bProcessed = true;
                if (!result->empty())
                {
                  ptJson->m["Response"] = new Json;
                  for (auto &i : *result)
                  {
                    ptJson->m["Response"]->push_back(i);
                  }
                }
                else
                {
                  strError = "No rows returned.";
                }
              }
              for (auto &i : *result)
              {
                i.clear();
              }
              result->clear();
              delete result;
            }
            else if (ptJson->m["Request"]->m.find("Update") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["Update"]->v.empty())
            {
              if (mysql("update", ptJson->m["User"]->v, ptJson->m["Password"]->v, ptJson->m["Server"]->v, unPort, ptJson->m["Database"]->v, ptJson->m["Request"]->m["Update"]->v, NULL, ullID, ullRows, strError))
              {
                stringstream ssRows;
                bProcessed = true;
                ptJson->m["Response"] = new Json;
                if (ullID > 0)
                {
                  stringstream ssID;
                  ssID << ullID;
                  ptJson->m["Response"]->insert("ID", ssID.str(), 'n');
                }
                ssRows << ullRows;
                ptJson->m["Response"]->insert("Rows", ssRows.str(), 'n');
              }
            }
            else
            {
              strError = "Please provide the Query or Update.";
            }
          }
          else
          {
            strError = "Please provide the Request.";
          }
        }
        else
        {
          strError = "Please provide the Database.";
        }
      }
      else
      {
        strError = "Please provide the Server.";
      }
    }
    else
    {
      strError = "Please provide the Password.";
    }
  }
  else
  {
    strError = "Please provide the User.";
  }
  ptJson->insert("Status", ((bProcessed)?"okay":"error"));
  if (!strError.empty())
  {
    ptJson->insert("Error", strError);
  }
  gMutexOutput.lock();
  gstrBuffer[1].append(ptJson->json(strJson)+"\n");
  gMutexOutput.unlock();
  delete ptJson;
  gMutexRequest.lock();
  if (gunRequests > 0)
  {
    gunRequests--;
  }
  gMutexRequest.unlock();
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
