// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : load.cpp
// author     : Ben Kietzman
// begin      : 2020-06-22
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

/*! \file load.cpp
* \brief Acorn Load
*
* Provides load testing capabilities.
*/
// {{{ includes
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <netdb.h>
#include <ncurses.h>
#include <poll.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
#include <Json>
using namespace common;
// }}}
// {{{ defines
#define BLUE   1
#define GREEN  2
#define RED    3
#define WHITE  4
#define YELLOW 5
// }}}
// {{{ structs
// {{{ conn
struct conn
{
  int fdConnect;
  int fdSocket;
  string strBuffer[2];
  string strError;
  string strRequest;
};
// }}}
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  string strLock = "/data/acorn/patterns.lock", strPatterns = "/data/acorn/patterns.json", strPort = "22676", strServer = "loadbalancer.web.att.com";
  WINDOW *pWindow = initscr();

  // {{{ command line arguments
  if (argc >= 2)
  {
    strServer = argv[1];
    if (argc >= 3)
    {
      strPort = argv[2];
    }
  }
  // }}}
  if (has_colors())
  {
    addrinfo hints, *result, *res;
    bool bConnected[3] = {false, false, false};
    int nReturn;
    // {{{ addrinfo
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((nReturn = getaddrinfo(strServer.c_str(), strPort.c_str(), &hints, &result)) == 0)
    {
      addrinfo *rp;
      int fdSocket;
      bConnected[0] = true;
      for (rp = result; !bConnected[2] && rp != NULL; rp = rp->ai_next)
      {
        bConnected[1] = false;
        if ((fdSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
        {
          bConnected[1] = true;
          if (connect(fdSocket, rp->ai_addr, rp->ai_addrlen) == 0)
          {
            bConnected[2] = true;
            res = rp;
          }
          close(fdSocket);
        }
      }
    }
    // }}}
    if (bConnected[2])
    {
      // {{{ prep
      bool bExit = false;
      char szBuffer[65536];
      int nKey, nReturn, X, Y;
      list<int> removals;
      map<string, string> patterns;
      map<string, list<conn *> > requests;
      pollfd *fds;
      rlimit tResourceLimit;
      size_t unIndex, unLine, unPosition, unThrottle = 10;
      string strError, strJson, strPrevError;
      stringstream ssMessage, ssTime;
      struct stat tStat;
      time_t CError, CPatterns = 0, CTime;
      Json *ptJson;
      // {{{ limit
      if (getrlimit(RLIMIT_NOFILE, &tResourceLimit) == 0)
      {
        tResourceLimit.rlim_cur = tResourceLimit.rlim_max;
        setrlimit(RLIMIT_NOFILE, &tResourceLimit);
      }
      // }}}
      // {{{ init screen
      curs_set(false);
      keypad(pWindow, true);
      nodelay(pWindow, true);
      noecho();
      getmaxyx(pWindow, Y, X);
      start_color();
      init_pair(BLUE, COLOR_BLUE, COLOR_BLACK);
      init_pair(GREEN, COLOR_GREEN, COLOR_BLACK);
      init_pair(RED, COLOR_RED, COLOR_BLACK);
      init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);
      init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);
      for (int y = 0; y < Y; y++)
      {
        for (int x = 0; x < X; x++)
        {
          attron(COLOR_PAIR(WHITE));
          mvaddch(y, x, ' ');
          attroff(COLOR_PAIR(WHITE));
          move(y, x);
        }
      }
      move(0, 0);
      // }}}
      // }}}
      while (!bExit || unIndex > 0)
      {
        ssMessage.str("");
        ssMessage << "THROTTLE:  " << unThrottle << setw(5) << setfill(' ') << " ";
        attron(COLOR_PAIR(BLUE));
        mvprintw(2, 1, "%s", ssMessage.str().c_str());
        attroff(COLOR_PAIR(BLUE));
        // {{{ patterns
        if (stat(strLock.c_str(), &tStat) == 0)
        {
          ssMessage.str("");
          ssMessage << "The patterns file is locked.";
          strError = ssMessage.str();
        }
        else if (stat(strPatterns.c_str(), &tStat) == 0)
        {
          if (CPatterns != tStat.st_mtime)
          {
            ifstream inPatterns;
            string strLine;
            stringstream ssJson;
            Json *ptPatterns;
            CPatterns = tStat.st_mtime;
            inPatterns.open(strPatterns.c_str());
            while (getline(inPatterns, strLine))
            {
              ssJson << strLine;
            }
            patterns.clear();
            ptPatterns = new Json(ssJson.str());
            ssMessage.str("");
            ssMessage << "PATTERNS:  ";
            for (map<string, Json *>::iterator i = ptPatterns->m.begin(); i != ptPatterns->m.end(); i++)
            {
              patterns[i->first] = i->second->json(strJson) + "\n";
              if (i != ptPatterns->m.begin())
              {
                ssMessage << ", ";
              }
              ssMessage << i->first;
            }
            delete ptPatterns;
            attron(COLOR_PAIR(GREEN));
            mvprintw(1, 1, "%s", ssMessage.str().c_str());
            attroff(COLOR_PAIR(GREEN));
          }
        }
        else
        {
          ssMessage.str("");
          ssMessage << "stat(" << errno << ") " << strerror(errno);
          strError = ssMessage.str();
        }
        // }}}
        // {{{ poll
        // {{{ prep
        if (!bExit)
        {
          for (map<string, string>::iterator i = patterns.begin(); i != patterns.end(); i++)
          {
            if (requests.find(i->first) == requests.end())
            {
              list<conn *> conns;
              requests[i->first] = conns;
            }
            if (patterns.find(i->first) != patterns.end())
            {
              while (requests[i->first].size() < unThrottle)
              {
                conn *ptConn = new conn;
                ptConn->fdConnect = -1;
                ptConn->fdSocket = -1;
                ptConn->strRequest = i->second;
                requests[i->first].push_back(ptConn);
              }
            }
          }
        }
        for (map<string, list<conn *> >::iterator i = requests.begin(); i != requests.end(); i++)
        {
          for (list<conn *>::iterator j = i->second.begin(); j != i->second.end(); j++)
          {
            if ((*j)->fdSocket == -1)
            {
              bool bGood = false;
              if ((*j)->fdConnect != -1)
              {
                if (connect((*j)->fdConnect, res->ai_addr, res->ai_addrlen) == 0)
                {
                  bGood = true;
                  (*j)->fdSocket = (*j)->fdConnect;
                  (*j)->fdConnect = -1;
                  (*j)->strBuffer[1] = (*j)->strRequest;
                }
                else if (errno != EALREADY && errno != EINPROGRESS)
                {
                  close((*j)->fdConnect);
                  (*j)->fdConnect = -1;
                  ssMessage.str("");
                  ssMessage << "connect(" << errno << ") " << strerror(errno);
                  (*j)->strError = ssMessage.str();
                }
              }
              else
              {
                if (((*j)->fdConnect = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) >= 0)
                {
                  long lArg;
                  if ((lArg = fcntl((*j)->fdConnect, F_GETFL, NULL)) >= 0)
                  {
                    bGood = true;
                    lArg |= O_NONBLOCK;
                    fcntl((*j)->fdConnect, F_SETFL, lArg);
                  }
                  else
                  {
                    close((*j)->fdConnect);
                    (*j)->fdConnect = -1;
                    ssMessage.str("");
                    ssMessage << "fcntl(" << errno << ") " << strerror(errno);
                    (*j)->strError = ssMessage.str();
                  }
                }
                else
                {
                  ssMessage.str("");
                  ssMessage << "socket(" << errno << ") " << strerror(errno);
                  (*j)->strError = ssMessage.str();
                }
              }
              if (!bGood && !(*j)->strError.empty())
              {
                strError = (*j)->strError;
                (*j)->strError.clear();
              }
            }
          }
        }
        unIndex = 0;
        for (map<string, list<conn *> >::iterator i = requests.begin(); i != requests.end(); i++)
        {
          for (list<conn *>::iterator j = i->second.begin(); j != i->second.end(); j++)
          {
            if ((*j)->fdSocket != -1)
            {
              unIndex++;
            }
          }
        }
        fds = new pollfd[unIndex];
        unIndex = 0;
        for (map<string, list<conn *> >::iterator i = requests.begin(); i != requests.end(); i++)
        {
          for (list<conn *>::iterator j = i->second.begin(); j != i->second.end(); j++)
          {
            if ((*j)->fdSocket != -1)
            {
              fds[unIndex].fd = (*j)->fdSocket;
              fds[unIndex].events = POLLIN;
              if (!(*j)->strBuffer[1].empty())
              {
                fds[unIndex].events |= POLLOUT;
              }
              unIndex++;
            }
          }
        }
        // }}}
        if ((nReturn = poll(fds, unIndex, 250)) > 0)
        {
          for (size_t i = 0; i < unIndex; i++)
          {
            bool bFound = false;
            for (map<string, list<conn *> >::iterator j = requests.begin(); !bFound && j != requests.end(); j++)
            {
              for (list<conn *>::iterator k = j->second.begin(); !bFound && k != j->second.end(); k++)
              {
                if (fds[i].fd == (*k)->fdSocket)
                {
                  bFound = true;
                  // {{{ read
                  if (fds[i].revents & POLLIN)
                  {
                    if ((nReturn = read(fds[i].fd, szBuffer, 65536)) > 0)
                    {
                      (*k)->strBuffer[0].append(szBuffer, nReturn);
                      if ((unPosition = (*k)->strBuffer[0].find("\n")) != string::npos)
                      {
                        ptJson = new Json((*k)->strBuffer[0].substr(0, unPosition));
                        (*k)->strBuffer[0].erase(0, (unPosition + 1));
                        if (ptJson->m.find("Status") == ptJson->m.end() || ptJson->m["Status"]->v != "okay")
                        {
                          if (ptJson->m.find("Error") != ptJson->m.end() && !ptJson->m["Error"]->v.empty())
                          {
                            strError = ptJson->m["Error"]->v;
                          }
                          else
                          {
                            strError = "Encountered an unknown error.";
                          }
                        }
                        delete ptJson;
                        removals.push_back(fds[i].fd);
                      }
                    }
                    else
                    {
                      removals.push_back(fds[i].fd);
                      if (nReturn < 0)
                      {
                        ssMessage.str("");
                        ssMessage << "read(" << errno << ") " << strerror(errno);
                        strError = ssMessage.str();
                      }
                    }
                  }
                  // }}}
                  // {{{ write
                  if (fds[i].revents & POLLOUT)
                  {
                    if ((nReturn = write(fds[i].fd, (*k)->strBuffer[1].c_str(), (*k)->strBuffer[1].size())) > 0)
                    {
                      (*k)->strBuffer[1].erase(0, nReturn);
                    }
                    else
                    {
                      removals.push_back(fds[i].fd);
                      if (nReturn < 0)
                      {
                        ssMessage.str("");
                        ssMessage << "write(" << errno << ") " << strerror(errno);
                        strError = ssMessage.str();
                      }
                    }
                  }
                  // }}}
                }
              }
            }
          }
        }
        else if (nReturn < 0)
        {
          bExit = true;
          ssMessage.str("");
          ssMessage << "poll(" << errno << ") " << strerror(errno);
          strError = ssMessage.str();
        }
        // {{{ post
        delete[] fds;
        removals.sort();
        removals.unique();
        while (!removals.empty())
        {
          bool bFound = false;
          for (map<string, list<conn *> >::iterator i = requests.begin(); !bFound && i != requests.end(); i++)
          {
            list<conn *>::iterator connIter = i->second.end();
            for (list<conn *>::iterator j = i->second.begin(); connIter == i->second.end() && j != i->second.end(); j++)
            {
              if ((*j)->fdSocket == removals.front())
              {
                connIter = j;
              }
            }
            if (connIter != i->second.end())
            {
              bFound = true;
              close((*connIter)->fdSocket);
              (*connIter)->fdSocket = -1;
              (*connIter)->strBuffer[0].clear();
              (*connIter)->strBuffer[1].clear();
              if (bExit || i->second.size() > unThrottle || (patterns.find(i->first) == patterns.end() || patterns[i->first] != (*connIter)->strRequest))
              {
                delete (*connIter);
                i->second.erase(connIter);
              }
            }
          }
          removals.pop_front();
        }
        // }}}
        // }}}
        // {{{ status
        unLine = 5;
        for (map<string, list<conn *> >::iterator i = requests.begin(); i != requests.end(); i++)
        {
          size_t unCount = 0;
          for (list<conn *>::iterator j = i->second.begin(); j != i->second.end(); j++)
          {
            if ((*j)->fdSocket != -1)
            {
              unCount++;
            }
          }
          attron(COLOR_PAIR(YELLOW));
          ssMessage.str("");
          ssMessage << setw(10) << setfill(' ') << i->first << ":  " << setw(3) << setfill(' ') << unCount << setw(10) << setfill(' ') << " ";
          mvprintw(unLine++, 1, "%s", ssMessage.str().c_str());
          attroff(COLOR_PAIR(YELLOW));
        }
        // }}}
        // {{{ error
        if (!strError.empty())
        {
          if (strError.size() > (size_t)(X-2))
          {
            strError.erase(X-2);
          }
          time(&CTime);
          if (strError != strPrevError)
          {
            strPrevError = strError;
            time(&CError);
            ssMessage.str("");
            ssMessage << "ERROR:  " << strError;
            attron(COLOR_PAIR(RED));
            mvprintw((patterns.size() + 7), 1, "%s", ssMessage.str().c_str());
            attroff(COLOR_PAIR(RED));
          }
          else if ((CTime - CError) >= 10)
          {
            strError.clear();
            strPrevError.clear();
            for (int x = 1; x < (X - 1); x++)
            {
              mvaddch((patterns.size() + 7), x, ' ');
            }
          }
        }
        // }}}
        // {{{ keyboard
        if ((nKey = getch()) != ERR)
        {
          switch (nKey)
          {
            case 113:
            {
              bExit = true;
              attron(COLOR_PAIR(WHITE));
              mvprintw((patterns.size() + 7), 1, "%s", "Exiting...                       ");
              attroff(COLOR_PAIR(WHITE));
              break;
            }
            case KEY_UP:
            {
              if (unThrottle < 1000)
              {
                unThrottle += 10;
              }
              break;
            }
            case KEY_DOWN:
            {
              if (unThrottle > 0)
              {
                unThrottle -= 10;
              }
              break;
            }
          }
        }
        // }}}
        refresh();
      }
      // {{{ post
      while (!requests.empty())
      {
        while (!requests.begin()->second.empty())
        {
          if (requests.begin()->second.front()->fdConnect != -1)
          {
            close(requests.begin()->second.front()->fdConnect);
          }
          if (requests.begin()->second.front()->fdSocket != -1)
          {
            close(requests.begin()->second.front()->fdSocket);
          }
          delete requests.begin()->second.front();
          requests.begin()->second.pop_front();
        }
        requests.erase(requests.begin());
      }
      patterns.clear();
      freeaddrinfo(result);
      // }}}
    }
    else if (!bConnected[0])
    {
      cerr << "ERROR:  getaddrinfo(" << nReturn << ") " << gai_strerror(nReturn) << endl;
    }
    else
    {
      freeaddrinfo(result);
      cerr << "ERROR:  " << ((!bConnected[1])?"socket":"connect") << "(" << errno << ") " << strerror(errno) << endl;
    }
    endwin();
  }
  else
  {
    endwin();
    cerr << "ERROR:  Your terminal does not support color." << endl;
  }

  return 0;
}
// }}}
