<!--
// vim600: fdm=marker
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : index.html
// author     : Ben Kietzman
// begin      : 2018-12-27
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
-->
<?php
function autoVersion($strFile)
{
  $strNewFile = $strFile;

  if (file_exists(dirname(__FILE__).'/'.$strFile))
  {
    $strNewFile = preg_replace('{\\.([^./]+)$}', '.'.filemtime(dirname(__FILE__).'/'.$strFile).".\$1", $strFile);
  }

  return $strNewFile;
}
?>
<html lang="en" ng-app="app">
  <head>
    <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
    <meta charset="utf-8">
    <title>Acorn</title>
    <meta name="viewport" content="width=device-width,initial-scale=1.0">
    <link rel="shortcut icon" href="/favicon.ico">
    <link rel="shortcut icon" href="/favicon.png">
    <link href="/node_modules/bootstrap/dist/css/bootstrap.min.css" rel="stylesheet">
    <link href="/include/common/css/common.css" rel="stylesheet">
    <link href="/include/common/css/sticky-footer.css" rel="stylesheet">
    <link href="/include/common/css/bootswatch/slate.css" rel="stylesheet">
    <script src="/node_modules/jquery/dist/jquery.min.js"></script>
    <script src="/node_modules/angular/angular.min.js"></script>
    <script src="/node_modules/angular-cookies/angular-cookies.min.js"></script>
    <script src="/node_modules/angular-route/angular-route.min.js"></script>
    <script src="/node_modules/angular-websocket/dist/angular-websocket.min.js"></script>
    <script src="/node_modules/bootstrap/dist/js/bootstrap.min.js"></script>
    <script src="/node_modules/angular-ui-bootstrap/dist/ui-bootstrap.js"></script>
    <script src="/node_modules/angular-ui-bootstrap/dist/ui-bootstrap-tpls.js"></script>
    <script src="/node_modules/ngstorage/ngStorage.min.js"></script>
    <script src="/include/common/auth/js/secure.js"></script>
    <script src="<?php echo autoVersion('app.js'); ?>"></script>
    <script src="<?php echo autoVersion('controller/Acorn.js'); ?>"></script>
    <script src="<?php echo autoVersion('factory/Acorn.js'); ?>"></script>
    <script src="/central/factory/Central.js"></script>
    <script src="/include/common/angularjs/commonController.js"></script>
    <script src="/include/common/angularjs/commonFactory.js"></script>
  </head>
  <body>
    <div id="wrap" class="container">
      <div ng-include="'/include/common/angularjs/menu.html'">
      </div>
      <div class="container">
        <div ng-include="'/include/common/angularjs/messages.html'">
        </div>
        <div ng-view>
        </div>
      </div>
    </div>
    <div ng-include="'/include/common/angularjs/bridgeStatus.html'">
    </div>
    <div ng-include="'/include/common/angularjs/footer.html'">
    </div>
    <div ng-include="'/central/template/CentralMenu.html'">
    </div>
  </body>
</html>

