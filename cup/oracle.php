#!/usr/bin/php
<?php
// vim600: fdm=marker
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : oracle.php
// author     : Ben Kietzman
// begin      : 2019-01-10
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

/*! \file oracle.php
* \brief Acorn Cup - oracle
*
* Provides the oracle cup for an acorn node.
*/
error_reporting(0);
// {{{ environment
for ($i = 1; $i < $argc; $i++)
{
  putenv($argv[$i]);
}
// }}}
// {{{ includes
include('/src/common/www/database/Database.php');
include('/scripts/common/www/database/Database.php');
// }}}
// {{{ variables
$bShutdown = false;
$database = new Database('oracle');
// }}}
// {{{ functions
// {{{ initialize()
function initialize()
{
  pcntl_signal(SIGHUP, "sighandle");
  pcntl_signal(SIGINT, "sighandle");
  pcntl_signal(SIGTERM, "sighandle");
}
// }}}
// {{{ sighandle()
function sighandle($nSignal)
{
  $bShutdown = true;
}
// }}}
// }}}
// {{{ main
initialize();
$bExit = false;
$nReturn = null;
$strBuffer = [null, null];
while (!$bShutdown && !$bExit)
{
  $read = [STDIN];
  $write = null;
  if ($strBuffer[1] != '')
  {
    $write = [STDOUT];
  }
  $error = null;
  if (($nReturn = stream_select($read, $write, $error, 2)) > 0)
  {
    if (in_array(STDIN, $read))
    {
      if (($strData = fread(STDIN, 65536)) !== false)
      {
        $strBuffer[0] .= $strData;
        while (($nPosition = strpos($strBuffer[0], "\n")) !== false)
        {
          $bProcessed = false;
          $json = json_decode(substr($strBuffer[0], 0, $nPosition), true);
          $strBuffer[0] = substr($strBuffer[0], ($nPosition + 1), (strlen($strBuffer[0]) - ($nPosition + 1)));
          $strError = null;
          if (isset($json['Schema']) && $json['Schema'] != '')
          {
            if (isset($json['Password']) && $json['Password'] != '')
            {
              if (isset($json['tnsName']) && $json['tnsName'] != '')
              {
                if (isset($json['Request']) && is_array($json['Request']))
                {
                  if ((isset($json['Request']['Query']) && $json['Request']['Query'] != '') || (isset($json['Request']['Update']) && $json['Request']['Update'] != ''))
                  {
                    $db = $database->connect($json['Schema'], $json['Password'], $json['tnsName']);
                    if (!$db->errorExist())
                    {
                      $strQuery = null;
                      if (isset($json['Request']['Query']) && $json['Request']['Query'] != '')
                      {
                        $strQuery = $json['Request']['Query'];
                      }
                      else if (isset($json['Request']['Update']) && $json['Request']['Update'] != '')
                      {
                        $strQuery = $json['Request']['Update'];
                      }
                      $query = $db->parse($strQuery);
                      if (!$query->errorExist())
                      {
                        if ($query->execute())
                        {
                          $bProcessed = true;
                          $json['Response'] = [];
                          if (isset($json['Request']['Query']) && $json['Request']['Query'] != '')
                          {
                            while (($row = $query->fetch('assoc')))
                            {
                              foreach ($row as $key => $value)
                              {
                                if (is_object($row[$key]))
                                {
                                  $lob = $row[$key]->load();
                                  $row[$key]->free();
                                  $row[$key] = base64_encode($lob);
                                }
                              }
                              $json['Response'][] = $row;
                            }
                          }
                          else if (isset($json['Request']['Update']) && $json['Request']['Update'] != '')
                          {
                            $json['Response']['Rows'] = $query->numRows();
                          }
                          $db->free($query);
                        }
                        else
                        {
                          $strError = $query->getError();
                        }
                      }
                      else
                      {
                        $strError = $query->getError();
                      }
                    }
                    else
                    {
                      $strError = $db->getError();
                    }
                    $database->disconnect($db);
                  }
                  else
                  {
                    $strError = 'Please provide the Query or Update within the Request.';
                  }
                }
                else
                {
                  $strError = 'Please provide the Request.';
                }
              }
              else
              {
                $strError = 'Please provide the tnsName.';
              }
            }
            else
            {
              $strError = 'Please provide the Password.';
            }
          }
          else
          {
            $strError = 'Please provide the Schema.';
          }
          $json['Status'] = (($bProcessed)?'okay':'error');
          if ($strError != '')
          {
            $json['Error'] = $strError;
          }
          $strBuffer[1] .= json_encode($json)."\n";
          unset($json);
        }
      }
      else
      {
        $bExit = true;
        fwrite(STDERR, 'fread('.socket_last_error().') error:  '.socket_strerror(socket_last_error())."\n");
      }
    }
    if (is_array($write) && in_array(STDOUT, $write))
    {
      if (($nReturn = fwrite(STDOUT, $strBuffer[1])) > 0)
      {
        $strBuffer[1] = substr($strBuffer[1], $nReturn, (strlen($strBuffer[1]) - $nReturn));
      }
      else
      {
        $bExit = true;
        fwrite(STDERR, 'fwrite('.socket_last_error().') error:  '.socket_strerror(socket_last_error())."\n");
      }
    }
  }
  else if ($nReturn === false)
  {
    $bExit = true;
    fwrite(STDERR, 'socket_select('.socket_last_error().') error:  '.socket_strerror(socket_last_error())."\n");
  }
  unset($read);
  unset($write);
}
// }}}
?>
