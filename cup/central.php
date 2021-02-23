#!/usr/bin/php
<?php
// vim600: fdm=marker
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : central.php
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

/*! \file central.php
* \brief Acorn Cup - central
*
* Provides the central cup for an acorn node.
*/
// {{{ includes
$strPath = '/src';
if ($argc == 2)
{
  $strPath = $argv[1];
}
require($strPath.'/common/www/Central.php');
// }}}
// {{{ variables
$bShutdown = false;
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
          if (isset($json['User']) && $json['User'] != '')
          {
            if (isset($json['Password']) && $json['Password'] != '')
            {
              if (isset($json['Database']) && $json['Database'] != '')
              {
                if (isset($json['Function']) && $json['Function'] != '')
                {
                  $strServer = null;
                  if (isset($json['Server']) && $json['Server'] != '')
                  {
                    $strServer = $json['Server'];
                  }
                  $bProcessed = true;
                  $central = new Central($json['User'], $json['Password'], $strServer, $json['Database']);
                  $central->errorHandler()->disable();
                  if (isset($json['Jwt']) && $json['Jwt'] != '')
                  {
                    if (!$central->loadJwt($json['Jwt'], $strError))
                    {
                      $bProcessed = false;
                    }
                  }
                  else if (isset($json['SessionID']) && $json['SessionID'] != '')
                  {
                    if (!$central->loadSession($json['SessionID'], $strError))
                    {
                      $bProcessed = false;
                    }
                  }
                  if ($bProcessed)
                  {
                    $bProcessed = false;
                    $request = null;
                    if (isset($json['Request']) && is_array($json['Request']))
                    {
                      $request = $json['Request'];
                    }
                    if (!is_array($request))
                    {
                      $request = array();
                    }
                    if (method_exists($central, $json['Function']))
                    {
                      $response = null;
                      if ($central->{$json['Function']}($request, $response, $strError))
                      {
                        $bProcessed = true;
                        if ($response != null)
                        {
                          $json['Response'] = $response;
                        }
                      }
                      unset($response);
                    }
                    else
                    {
                      $strError = 'Please provide a valid Function.';
                    }
                    unset($request);
                  }
                }
                else
                {
                  $strError = 'Please provide the Function.';
                }
              }
              else
              {
                $strError = 'Please provide the Database.';
              }
            }
            else
            {
              $strError = 'Please provide the Password.';
            }
          }
          else
          {
            $strError = 'Please provide the User.';
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
