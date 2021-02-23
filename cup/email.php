#!/usr/bin/php
<?php
// vim600: fdm=marker
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : email.php
// author     : Ben Kietzman
// begin      : 2019-01-04
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

/*! \file email.php
* \brief Acorn Cup - email
*
* Provides the email cup for an acorn node.
*/
// {{{ includes
require('Mail.php');
require('Mail/mime.php');
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
          if (isset($json['Request']) && is_array($json['Request']))
          {
            if (isset($json['Request']['To']) && $json['Request']['To'] != '')
            {
              $header = [];
              if (isset($json['Request']['From']) && $json['Request']['From'] != '')
              {
                $header['From'] = $json['Request']['From'];
              }
              if (isset($json['Request']['CC']) && $json['Request']['CC'] != '')
              {
                $header['Cc'] = $json['Request']['CC'];
              }
              if (isset($json['Request']['BCC']) && $json['Request']['BCC'] != '')
              {
                $header['Bcc'] = $json['Request']['BCC'];
              }
              if (isset($json['Request']['Subject']) && $json['Request']['Subject'] != '')
              {
                $header['Subject'] = $json['Request']['Subject'];
              }
              $mime = new Mail_mime(array('eol' => "\n"));
              if (isset($json['Request']['Text']) && $json['Request']['Text'] != '')
              {
                $mime->setTXTBody($json['Request']['Text']);
              }
              if (isset($json['Request']['HTML']) && $json['Request']['HTML'] != '')
              {
                $mime->setHTMLBody($json['Request']['HTML']);
              }
              if (isset($json['Request']['Attachments']) && is_array($json['Request']['Attachments']))
              {
                $nSize = sizeof($json['Request']['Attachments']);
                for ($i = 0; $i < $nSize; $i++)
                {
                  if (isset($json['Request']['Attachments'][$i]['Data']) && isset($json['Request']['Attachments'][$i]['Name']) && $json['Request']['Attachments'][$i]['Name'] != '')
                  {
                    if (!isset($json['Request']['Attachments'][$i]['Type']) || $json['Request']['Attachments'][$i]['Type'] == '')
                    {
                      $json['Request']['Attachments'][$i]['Type'] = 'application/octet-stream';
                    }
                    $mime->addAttachment(((isset($json['Request']['Attachments'][$i]['Encode']) && $json['Request']['Attachments'][$i]['Encode'] == 'base64')?base64_decode($json['Request']['Attachments'][$i]['Data']):$json['Request']['Attachments'][$i]['Data']), $json['Request']['Attachments'][$i]['Type'], $json['Request']['Attachments'][$i]['Name'], false);
                  }
                }
              }
              $body = $mime->get();
              $headers = $mime->headers($header);
              unset($header);
              $mail = Mail::factory('mail');
              if ($mail->send($json['Request']['To'], $headers, $body))
              {
                $bProcessed = true;
              }
              else
              {
                $strError = 'Could not send the email!';
              }
            }
            else
            {
              $strError = 'Please provide the comma separated To email addresses.';
            }
          }
          else
          {
            $strError = 'Please provide the Request.';
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
