// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : password.cpp
// author     : Ben Kietzman
// begin      : 2019-01-14
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

/*! \file password.cpp
* \brief Acorn Cup - password
*
* Provides the password cup for an acorn node.
*/
// {{{ includes
#include <iostream>
#include <poll.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
using namespace std;
#include <Bridge>
#include <Central>
#include <Json>
#include <SignalHandling>
using namespace common;
// }}}
// {{{ defines
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -c CONF, --conf=CONF" << endl << "     Sets the configuration file." << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -m RESIDENT, --memory=RESIDENT" << endl << "     Provides the maximum resident memory size restriction in MB." << endl << endl
// }}}
// {{{ global variables
bool gbShutdown = false; //!< Global shutdown variable.
mutex gMutexRequest; //!< Global request mutex.
mutex gMutexOutput; //!< Global output mutex.
size_t gunRequests = 0; //!< Global requests.
string gstrApplication = "Acorn"; //!< Global application name.
string gstrBuffer[2]; //!< Global buffers.
string gstrData = "/data/acorn"; //!< Global data directory.
string gstrName; //!< Global acorn or gateway name.
Central *gpCentral = NULL; //!< Contains the Central class.
// }}}
// {{{ prototypes
/*! \fn string escape(const string strIn, string &strOut)
* \brief Escapes a MySQL value.
* \param strIn Input string.
* \param strOut Output string.
* \return Returns the escaped string.
*/
string escape(const string strIn, string &strOut);
/*! \fn void initialize(string strPrefix, int argc, char *argv[], string &strError)
* \brief Monitors the health of the running process.
* \param strPrefix Contains the function prefix.
* \param argc Contains the number of command-line arguments.
* \param argv Contains the command-line arguments.
* \return Returns a boolean true/false value.
*/
bool initialize(string strPrefix, int argc, char *argv[], string &strError);
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
      }
      gbShutdown = true;
      while (gunRequests > 0)
      {
        usleep(250000);
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
// {{{ escape()
string escape(const string strIn, string &strOut)
{
  size_t unSize = strIn.size();
  stringstream ssResult;
  
  for (size_t i = 0; i < unSize; i++)
  { 
    switch (strIn[i])
    {
      case 0    : ssResult << "\\0";  break;
      case '\n' : ssResult << "\\n";  break;
      case '\r' : ssResult << "\\r";  break;
      case '\\' : ssResult << "\\\\"; break;
      case '\'' : ssResult << "\\'";  break;
      case '"'  : ssResult << "\\\""; break;
      default   : ssResult << strIn[i];
    };
  }
  
  return (strOut = ssResult.str());
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
      else
      {
        bResult = false;
        strError = (string)"Illegal option \"" + strArg + (string)"\".";
        mUSAGE(argv[0]);
      }
    }
    // }}}
    gpCentral->acorn()->useSingleSocket(true);
    gpCentral->setApplication(gstrApplication);
    gpCentral->setLog(gstrData, "password_", "monthly", true, true);
    gpCentral->setRoom("#system");
  }

  return bResult;
}
// }}}
// {{{ request()
void request(Json *ptJson)
{
  bool bProcessed = false;
  string strError, strJson;

  if (ptJson->m.find("Function") != ptJson->m.end())
  {
    if (ptJson->m["Function"]->v == "delete" || ptJson->m["Function"]->v == "update" || ptJson->m["Function"]->v == "verify")
    {
      if (ptJson->m["Function"]->v != "update" || (ptJson->m.find("Request") != ptJson->m.end() && ptJson->m["Request"]->m.find("NewPassword") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["NewPassword"]->v.empty()))
      {
        if (ptJson->m.find("Application") != ptJson->m.end() && !ptJson->m["Application"]->v.empty())
        {
          if (ptJson->m.find("User") != ptJson->m.end() && !ptJson->m["User"]->v.empty())
          {
            if (ptJson->m.find("Password") != ptJson->m.end() && !ptJson->m["Password"]->v.empty())
            {
              ifstream inFile;
              string strConf = "/etc/central.conf";
              Json *ptConf = NULL;
              if (ptJson->m.find("Conf") != ptJson->m.end() && !ptJson->m["Conf"]->v.empty())
              {
                strConf = ptJson->m["Conf"]->v;
              }
              inFile.open(strConf.c_str());
              if (inFile.good())
              {
                string strLine;
                if (getline(inFile, strLine))
                {
                  ptConf = new Json(strLine);
                }
              }
              inFile.close();
              if (ptConf != NULL)
              {
                bool bBridge = false;
                if (ptJson->m["Function"]->v == "verify" && ptConf->m.find("Bridge Password") != ptConf->m.end() && !ptConf->m["Bridge Password"]->v.empty() && ptConf->m.find("Bridge Port") != ptConf->m.end() && !ptConf->m["Bridge Port"]->v.empty() && ptConf->m.find("Bridge Server") != ptConf->m.end() && !ptConf->m["Bridge Server"]->v.empty() && ptConf->m.find("Bridge User") != ptConf->m.end() && !ptConf->m["Bridge User"]->v.empty())
                {
                  Bridge bridge(strError);
                  bridge.setCredentials(ptConf->m["Bridge User"]->v, ptConf->m["Bridge Password"]->v);
                  if (strError.empty())
                  { 
                    bBridge = true;
                    if (bridge.passwordVerify(ptJson->m["Application"]->v, ptJson->m["Type"]->v, ptJson->m["User"]->v, ptJson->m["Password"]->v, strError))
                    {
                      bProcessed = true;
                    }
                    else if (strError == "Bridge request failed without returning an error message.")
                    {
                      bBridge = false;
                    }
                  }
                }
                if (!bBridge)
                {
                  if (ptConf->m.find("Database") != ptConf->m.end() && !ptConf->m["Database"]->v.empty())
                  {
                    if (ptConf->m.find("Database Password") != ptConf->m.end() && !ptConf->m["Database Password"]->v.empty())
                    {
                      if (ptConf->m.find("Database Server") != ptConf->m.end() && !ptConf->m["Database Server"]->v.empty())
                      {
                        if (ptConf->m.find("Database User") != ptConf->m.end() && !ptConf->m["Database User"]->v.empty())
                        {
                          list<map<string, string> > getAccount;
                          string strValue;
                          stringstream ssQuery;
                          ssQuery << "select b.aes, b.encrypt, b.id, b.password";
                          if (ptConf->m.find("Aes") != ptConf->m.end() && !ptConf->m["Aes"]->v.empty())
                          {
                            ssQuery << ", aes_decrypt(from_base64(b.password), sha2('" << escape(ptConf->m["Aes"]->v, strValue) << "', 512)) decrypted_password";
                          }
                          ssQuery << ", c.type from application a, application_account b, account_type c where a.id=b.application_id and b.type_id = c.id and a.name = '" << escape(ptJson->m["Application"]->v, strValue) << "' and b.user_id = '" << escape(ptJson->m["User"]->v, strValue) << "'";
                          if (ptJson->m.find("Type") != ptJson->m.end() && !ptJson->m["Type"]->v.empty())
                          {
                            ssQuery << " and c.type = '" << escape(ptJson->m["Type"]->v, strValue) << "'";
                          }
                          if (gpCentral->acorn()->mysqlQuery(ptConf->m["Database User"]->v, ptConf->m["Database Password"]->v, ptConf->m["Database Server"]->v, ptConf->m["Database"]->v, ssQuery.str(), getAccount, strError))
                          {
                            if (getAccount.size() == 1)
                            {
                              bool bVerified = false;
                              map<string, string> getAccountRow = getAccount.front();
                              if (getAccountRow["encrypt"] == "1")
                              {
                                list<map<string, string> > getAccountID;
                                ssQuery.str("");
                                ssQuery << "select id from application_account where id = " << getAccountRow["id"] << " and `password` = concat('*',upper(sha1(unhex(sha1('" << escape(ptJson->m["Password"]->v, strValue) << "')))))";
                                if (gpCentral->acorn()->mysqlQuery(ptConf->m["Database User"]->v, ptConf->m["Database Password"]->v, ptConf->m["Database Server"]->v, ptConf->m["Database"]->v, ssQuery.str(), getAccountID, strError))
                                {
                                  if (!getAccountID.empty())
                                  {
                                    bVerified = true;
                                  }
                                }
                                for (list<map<string, string> >::iterator getAccountIDIter = getAccountID.begin(); getAccountIDIter != getAccountID.end(); getAccountIDIter++)
                                {
                                  getAccountIDIter->clear();
                                }
                                getAccountID.clear();
                              }
                              else if (getAccountRow["aes"] == "1")
                              {
                                if (getAccountRow.find("decrypted_password") != getAccountRow.end() && getAccountRow["decrypted_password"] == ptJson->m["Password"]->v)
                                {
                                  bVerified = true;
                                }
                              }
                              else if (getAccountRow["password"] == ptJson->m["Password"]->v)
                              {
                                bVerified = true;
                              }
                              if (bVerified)
                              {
                                if (ptJson->m["Function"]->v == "delete")
                                {
                                  ssQuery.str("");
                                  ssQuery << "delete from application_account where id = " << getAccountRow["id"];
                                  if (gpCentral->acorn()->mysqlUpdate(ptConf->m["Database User"]->v, ptConf->m["Database Password"]->v, ptConf->m["Database Server"]->v, ptConf->m["Database"]->v, ssQuery.str(), strError))
                                  {
                                    bProcessed = true;
                                  }
                                  else
                                  {
                                    stringstream ssError;
                                    ssError << "Acorn::mysqlUpdate(" << ssQuery.str() << ") error:  " << strError;
                                    strError = ssError.str();
                                  }
                                }
                                else if (ptJson->m["Function"]->v == "update")
                                {
                                  ssQuery.str("");
                                  ssQuery << "update application_account set `password` = ";
                                  if (getAccountRow["encrypt"] == "1")
                                  {
                                    ssQuery << "concat('*',upper(sha1(unhex(sha1('" << escape(ptJson->m["Request"]->m["NewPassword"]->v, strValue) << "')))))";
                                  }
                                  else if (ptConf->m.find("Aes") != ptConf->m.end() && !ptConf->m["Aes"]->v.empty())
                                  {
                                    ssQuery << "to_base64(aes_encrypt('" << escape(ptJson->m["Request"]->m["NewPassword"]->v, strValue) << "', sha2('" << escape(ptConf->m["Aes"]->v, strValue) << "', 512))), aes = 1";
                                  }
                                  else
                                  {
                                    ssQuery << "'" << escape(ptJson->m["Request"]->m["NewPassword"]->v, strValue) << "'";
                                  }
                                  ssQuery << " where id = " << getAccountRow["id"];
                                  if (gpCentral->acorn()->mysqlUpdate(ptConf->m["Database User"]->v, ptConf->m["Database Password"]->v, ptConf->m["Database Server"]->v, ptConf->m["Database"]->v, ssQuery.str(), strError))
                                  {
                                    bProcessed = true;
                                  }
                                  else
                                  {
                                    stringstream ssError;
                                    ssError << "Acorn::mysqlUpdate(HIDDEN) error:  " << strError;
                                    strError = ssError.str();
                                  }
                                }
                                else if (ptJson->m["Function"]->v == "verify")
                                {
                                  bProcessed = true;
                                }
                              }
                              else
                              {
                                strError = "Failed password verification.";
                              }
                              getAccountRow.clear();
                            }
                            else if (getAccount.empty())
                            {
                              strError = "Failed to find the account.";
                            }
                            else
                            {
                              stringstream ssError;
                              ssError << getAccount.size() << " accounts match this criteria.";
                              strError = ssError.str();
                            }
                          }
                          else
                          {
                            stringstream ssError;
                            ssError << "Acorn::mysqlQuery(" << ssQuery.str() << ") error:  " << strError << endl;
                            strError = ssError.str();
                          }
                          for (list<map<string, string> >::iterator getAccountIter = getAccount.begin(); getAccountIter != getAccount.end(); getAccountIter++)
                          {
                            getAccountIter->clear();
                          }
                          getAccount.clear();
                        }
                        else
                        {
                          strError = (string)"Failed to read the Database User field from the " + strConf + (string)" file.";
                        }
                      }
                      else
                      {
                        strError = (string)"Failed to read the Database Server field from the " + strConf + (string)" file.";
                      }
                    }
                    else
                    {
                      strError = (string)"Failed to read the Database Password field from the " + strConf + (string)" file.";
                    }
                  }
                  else
                  {
                    strError = (string)"Failed to read the Database field from the " + strConf + (string)" file.";
                  }
                }
                delete ptConf;
              }
              else
              {
                strError = (string)"Failed to load the " + strConf + (string)" file for reading.";
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
        }
        else
        {
          strError = "Please provide the Application.";
        }
      }
      else
      {
        strError = "Please provide the NewPassword within the Request.";
      }
    }
    else
    {
      strError = "Please provide a valid Function:  delete, update, verify";
    }
  }
  else
  {
    strError = "Please provide the Function.";
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
