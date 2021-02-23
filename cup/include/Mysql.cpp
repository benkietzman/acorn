// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : Mysql.cpp
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

/*! \file Mysql.cpp
* \brief MySQL Interface
*
* Provides an interface to MySQL.
*/
// {{{ includes
#include "Mysql"
// }}}
extern "C++"
{
  namespace acorn
  {
    // {{{ Mysql()
    Mysql::Mysql(const size_t unMaxConnections, const size_t unMaxQueue)
    {
      m_unMaxConnections = unMaxConnections;
      m_unMaxQueue = unMaxQueue;
    }
    // }}}
    // {{{ ~Mysql()
    Mysql::~Mysql()
    {
      for (map<string, list<acorn_mysql *> >::iterator i = m_conn.begin(); i != m_conn.end(); i++)
      {
        for (list<acorn_mysql *>::iterator j = i->second.begin(); j != i->second.end(); j++)
        {
          mysql_close((*j)->conn);
          delete (*j);
        }
        i->second.clear();
      }
      m_conn.clear();
    }
    // }}}
    // {{{ conn()
    map<string, list<acorn_mysql *> > *Mysql::conn()
    {
      return &m_conn;
    }
    // }}}
    // {{{ connect()
    bool Mysql::connect(const string strUser, const string strPassword, const string strServer, const unsigned int unPort, const string strDatabase, list<acorn_mysql *>::iterator &iter, string &strError)
    {
      bool bResult = false;
      string strName;
      stringstream ssError, ssName;

      ssName << strUser << '|' << strPassword << '|' << strServer << '|' << unPort << '|' << strDatabase;
      strName = ssName.str();
      lock();
      if (m_conn.find(strName) == m_conn.end())
      { 
        list<acorn_mysql *> mysqlList;
        m_conn[strName] = mysqlList;
      }   
      if (m_conn.find(strName) != m_conn.end())
      { 
        iter = m_conn[strName].end();
        for (list<acorn_mysql *>::iterator i = m_conn[strName].begin(); iter == m_conn[strName].end() && i != m_conn[strName].end(); i++)
        {
          if ((*i)->unThreads < m_unMaxQueue)
          {
            iter = i;
          }
        }
        if (iter == m_conn[strName].end() && m_conn[strName].size() >= m_unMaxConnections)
        {
          iter = m_conn[strName].begin();
          for (list<acorn_mysql *>::iterator i = m_conn[strName].begin(); i != m_conn[strName].end(); i++)
          {
            if ((*i)->unThreads < (*iter)->unThreads)
            {
              iter = i;
            }
          }
        }
        if (iter == m_conn[strName].end())
        {
          bool bConnected = false;
          acorn_mysql *ptMysql = new acorn_mysql;
          if ((ptMysql->conn = mysql_init(NULL)) != NULL)
          {
            unsigned int unTimeout = 2;
            mysql_options(ptMysql->conn, MYSQL_OPT_CONNECT_TIMEOUT, &unTimeout);
            if (mysql_real_connect(ptMysql->conn, strServer.c_str(), strUser.c_str(), strPassword.c_str(), strDatabase.c_str(), unPort, NULL, 0) != NULL)
            {
              bConnected = true;
            }
            else
            {
              ssError.str("");
              ssError << "mysql_real_connect(" << mysql_errno(ptMysql->conn) << ") [" << strServer << "," << strUser << "," << strDatabase << "]:  " << mysql_error(ptMysql->conn);
              strError = ssError.str();
              mysql_close(ptMysql->conn);
            }
          }
          else
          {
            strError = "mysql_init():  Failed to initialize MySQL library.";
          }
          if (bConnected)
          {
            ptMysql->unThreads = 0;
            m_conn[strName].push_back(ptMysql);
            iter = m_conn[strName].end();
            iter--;
          }
          else
          {
            delete ptMysql;
          }
        }
        if (iter != m_conn[strName].end())
        {
          bResult = true;
          (*iter)->unThreads++;
          time(&((*iter)->CTime));
        }
      }
      else
      {
        strError = "Failed to insert database name into connection pool.";
      }
      unlock();

      return bResult;
    }
    // }}}
    // {{{ disconnect()
    void Mysql::disconnect(const bool bUseLock)
    {
      list<map<string, list<acorn_mysql *> >::iterator> removeName;
      time_t CTime;

      time(&CTime);
      if (bUseLock)
      {
        lock();
      }
      for (map<string, list<acorn_mysql *> >::iterator i = m_conn.begin(); i != m_conn.end(); i++)
      {
        list<list<acorn_mysql *>::iterator> removeConn;
        for (list<acorn_mysql *>::iterator j = i->second.begin(); j != i->second.end(); j++)
        {
          if ((*j)->unThreads == 0 && (CTime - (*j)->CTime) > 60)
          {
            removeConn.push_back(j);
          }
        }
        for (list<list<acorn_mysql *>::iterator>::iterator j = removeConn.begin(); j != removeConn.end(); j++)
        {
          mysql_close((*(*j))->conn);
          delete (*(*j));
          i->second.erase(*j);
          if (i->second.empty())
          {
            removeName.push_back(i);
          }
        }
        removeConn.clear();
      }
      for (list<map<string, list<acorn_mysql *> >::iterator>::iterator i = removeName.begin(); i != removeName.end(); i++)
      {
        m_conn.erase(*i);
      }
      if (bUseLock)
      {
        unlock();
      }
      removeName.clear();
    }
    void Mysql::disconnect(list<acorn_mysql *>::iterator &iter)
    {
      lock();
      (*iter)->unThreads--;
      disconnect(false);
      unlock();
    }
    // }}}
    // {{{ fetch()
    map<string, string> *Mysql::fetch(MYSQL_RES *result, vector<string> fields)
    {
      map<string, string> *pRow = NULL;
      MYSQL_ROW row;

      if ((row = mysql_fetch_row(result)))
      {
        map<string, string> rowMap;
        for (unsigned int i = 0; i < fields.size(); i++)
        {
          rowMap[fields[i]] = (row[i] != NULL)?row[i]:"";
        }
        pRow = new map<string, string>(rowMap);
        rowMap.clear();
      }

      return pRow;
    }
    // }}}
    // {{{ fields()
    bool Mysql::fields(MYSQL_RES *result, vector<string> &fields)
    {
      bool bResult = false;
      MYSQL_FIELD *field;

      while ((field = mysql_fetch_field(result)) != NULL)
      {
        string strValue;
        bResult = true;
        strValue.assign(field->name, field->name_length);
        fields.push_back(strValue);
      }

      return bResult;
    }
    // }}}
    // {{{ free()
    void Mysql::free(MYSQL_RES *result)
    {
      mysql_free_result(result);
    }
    // }}}
    // {{{ lock()
    void Mysql::lock()
    {
      m_mutex.lock();
    }
    // }}}
    // {{{ query()
    MYSQL_RES *Mysql::query(list<acorn_mysql *>::iterator &iter, const string strQuery, unsigned long long &ullRows, string &strError)
    {
      bool bRetry = true;
      size_t unAttempt = 0;
      stringstream ssError;
      MYSQL_RES *result = NULL;

      while (bRetry && unAttempt++ < 10)
      {
        bRetry = false;
        if (mysql_query((*iter)->conn, strQuery.c_str()) == 0)
        {
          if ((result = mysql_store_result((*iter)->conn)) != NULL)
          {
            ullRows = mysql_num_rows(result);
          }
          else
          {
            ssError.str("");
            ssError << "mysql_store_result(" << mysql_errno((*iter)->conn) << "):  " << mysql_error((*iter)->conn);
            strError = ssError.str();
          }
        }
        else
        {
          ssError.str("");
          ssError << "mysql_query(" << mysql_errno((*iter)->conn) << "):  " << mysql_error((*iter)->conn);
          strError = ssError.str();
          if (strError.find("try restarting transaction") != string::npos)
          {
            bRetry = true;
          }
        }
        if (bRetry)
        {
          usleep(250000);
        }
      }

      return result;
    }
    // }}}
    // {{{ unlock()
    void Mysql::unlock()
    {
      m_mutex.unlock();
    }
    // }}}
    // {{{ update()
    bool Mysql::update(list<acorn_mysql *>::iterator &iter, const string strQuery, unsigned long long &ullID, unsigned long long &ullRows, string &strError)
    {
      bool bResult = false, bRetry = true;
      size_t unAttempt = 0;
      stringstream ssError;

      while (bRetry && unAttempt++ < 10)
      {
        bRetry = false;
        if (mysql_real_query((*iter)->conn, strQuery.c_str(), strQuery.size()) == 0)
        {
          bResult = true;
          ullID = mysql_insert_id((*iter)->conn);
          ullRows = mysql_affected_rows((*iter)->conn);
        }
        else
        {
          ssError.str("");
          ssError << "mysql_real_query(" << mysql_errno((*iter)->conn) << "):  " << mysql_error((*iter)->conn);
          strError = ssError.str();
          if (strError.find("try restarting transaction") != string::npos)
          {
            bRetry = true;
          }
        }
        if (bRetry)
        {
          usleep(250000);
        }
      }

      return bResult;
    }
    // }}}
  }
}
