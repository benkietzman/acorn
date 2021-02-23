// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : jwt.cpp
// author     : Ben Kietzman
// begin      : 2019-01-08
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

/*! \file jwt.cpp
* \brief Acorn Cup - jwt
*
* Provides the jwt cup for an acorn node.
*/
// {{{ includes
#include <iostream>
#include <poll.h>
#include <sstream>
#include <string>
#include <unistd.h>
using namespace std;
#include <jwt/jwt_all.h>
using json = nlohmann::json;
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
                      if (ptJson->m["Request"]->m.find("Signer") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["Signer"]->v.empty())
                      {
                        bool bDecode = false;
                        MessageSigner *pSigner = NULL;
                        if (ptJson->m["Request"]->m["Signer"]->v == "HS256" || ptJson->m["Request"]->m["Signer"]->v == "HS384" || ptJson->m["Request"]->m["Signer"]->v == "HS512")
                        {
                          if (ptJson->m["Request"]->m.find("Secret") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["Secret"]->v.empty())
                          {
                            bDecode = true;
                            if (ptJson->m["Request"]->m["Signer"]->v == "HS256")
                            {
                              pSigner = new HS256Validator(ptJson->m["Request"]->m["Secret"]->v);
                            }
                            else if (ptJson->m["Request"]->m["Signer"]->v == "HS384")
                            {
                              pSigner = new HS384Validator(ptJson->m["Request"]->m["Secret"]->v);
                            }
                            else if (ptJson->m["Request"]->m["Signer"]->v == "HS512")
                            {
                              pSigner = new HS512Validator(ptJson->m["Request"]->m["Secret"]->v);
                            }
                          }
                          else
                          {
                            strError = "Please provide the Secret.";
                          }
                        }
                        else if (ptJson->m["Request"]->m["Signer"]->v == "RS256" || ptJson->m["Request"]->m["Signer"]->v == "RS384" || ptJson->m["Request"]->m["Signer"]->v == "RS512")
                        {
                          if (ptJson->m["Request"]->m.find("Public Key") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["Public Key"]->v.empty())
                          {
                            if (ptJson->m["Request"]->m.find("Private Key") != ptJson->m["Request"]->m.end() && !ptJson->m["Request"]->m["Private Key"]->v.empty())
                            {
                              bDecode = true;
                              if (ptJson->m["Request"]->m["Signer"]->v == "RS256")
                              {
                                pSigner = new RS256Validator(ptJson->m["Request"]->m["Public Key"]->v, ptJson->m["Request"]->m["Private Key"]->v);
                              }
                              else if (ptJson->m["Request"]->m["Signer"]->v == "RS384")
                              {
                                pSigner = new RS384Validator(ptJson->m["Request"]->m["Public Key"]->v, ptJson->m["Request"]->m["Private Key"]->v);
                              }
                              else if (ptJson->m["Request"]->m["Signer"]->v == "RS512")
                              {
                                pSigner = new RS512Validator(ptJson->m["Request"]->m["Public Key"]->v, ptJson->m["Request"]->m["Private Key"]->v);
                              }
                            }
                            else
                            {
                              if (ptJson->m["Request"]->m["Signer"]->v == "RS256")
                              {
                                pSigner = new RS256Validator(ptJson->m["Request"]->m["Public Key"]->v);
                              }
                              else if (ptJson->m["Request"]->m["Signer"]->v == "RS384")
                              {
                                pSigner = new RS384Validator(ptJson->m["Request"]->m["Public Key"]->v);
                              }
                              else if (ptJson->m["Request"]->m["Signer"]->v == "RS512")
                              {
                                pSigner = new RS512Validator(ptJson->m["Request"]->m["Public Key"]->v);
                              }
                            }
                          }
                          else
                          {
                            strError = "Please provide the Public Key.";
                          }
                        }
                        if (pSigner != NULL)
                        {
                          if (ptJson->m["Request"]->m.find("Payload") != ptJson->m["Request"]->m.end())
                          {
                            if (ptJson->m.find("Function") != ptJson->m.end() && !ptJson->m["Function"]->v.empty())
                            {
                              if (ptJson->m["Function"]->v == "decode")
                              {
                                if (bDecode)
                                {
                                  ExpValidator exp;
                                  json header, payload;
                                  try
                                  {
                                    stringstream ssHeader, ssPayload;
                                    bProcessed = true;
                                    ptJson->m["Response"] = new Json;
                                    tie(header, payload) = JWT::Decode(ptJson->m["Request"]->m["Payload"]->v, pSigner, &exp);
                                    ssHeader << header;
                                    ptJson->m["Response"]->m["Header"] = new Json(ssHeader.str());
                                    ssPayload << payload;
                                    ptJson->m["Response"]->m["Payload"] = new Json(ssPayload.str());
                                  }
                                  catch (InvalidTokenError &tfe)
                                  {
                                    bProcessed = false;
                                    strError = tfe.what();
                                  }
                                  catch (exception &e)
                                  {
                                    bProcessed = false;
                                    strError = e.what();
                                  }
                                }
                                else
                                {
                                  strError = "Please provide a Signer capable of decoding.";
                                }
                              }
                              else if (ptJson->m["Function"]->v == "encode")
                              {
                                json data;
                                try
                                {
                                  stringstream ssJson(ptJson->m["Request"]->m["Payload"]->json(strJson));
                                  bProcessed = true;
                                  ptJson->m["Response"] = new Json;
                                  ssJson >> data;
                                  ptJson->m["Response"]->insert("Payload", JWT::Encode((*pSigner), data));
                                }
                                catch (exception &e)
                                {
                                  bProcessed = false;
                                  strError = e.what();
                                }
                              }
                              else
                              {
                                strError = "Please provide a valid Function:  decode, encode.";
                              }
                            }
                            else
                            {
                              strError = "Please provide the Function.";
                            }
                          }
                          else
                          {
                            strError = "Please provide the Payload.";
                          }
                          delete pSigner;
                        }
                        else if (strError.empty())
                        {
                          strError = "Please provide a valid Signer:  HS256, HS384, HS512, RS256, RS384, RS512.";
                        }
                      }
                      else
                      {
                        strError = "Please provide the Signer.";
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
    gpCentral->setLog(gstrData, "jwt_", "monthly", true, true);
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
