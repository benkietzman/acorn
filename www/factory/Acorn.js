// vim600: fdm=marker
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : Acorn.js
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

var factories = {};
factories.acorn = function ($cookies, $http, $location, $q, $rootScope, $websocket, common)
{
  var factory = {};
  // {{{ variables
  factory.m_bCentralMenuReady = false;
  factory.m_bCommonAuthReady = false;
  factory.m_bCommonWsReady = false;
  factory.m_bFooterReady = false;
  factory.m_bReady = false;
  factory.m_strApplication = 'Acorn';
  // }}}
  // {{{ ready()
  factory.ready = function (response, error)
  {       
    if (!this.m_bReady && this.m_bCentralMenuReady && this.m_bCommonAuthReady && this.m_bCommonWsReady && this.m_bFooterReady)
    {
      this.m_bReady = true;
      $rootScope.$root.$broadcast('ready', null);
    }   
    
    return this.m_bReady;
  };
  // }}}
  // {{{ request() 
  factory.request = function (strService, strFunction, request, callback)
  { 
    var data = {'Service': strService, 'Function': strFunction};
    if (request != null)
    {
      data.Arguments = request;
    }
    $http.post('/acorn/include/request.php', data).then(callback);
  };
  // }}}
  // {{{ response()
  factory.response = function (response, error)
  {
    var bResult = false;

    if (response.status == 200)
    {
      if (response.data.Status && response.data.Status == 'okay')
      {
        bResult = true;
      }
      else if (error != null)
      {
        if (response.data.Error)
        {
          error.message = response.data.Error;
        }
        else
        {
          error.message = 'Encountered an unknown error.';
        }
      }
    }
    else if (error != null)
    {
      error.message = response.status;
    }

    return bResult;
  };
  // }}}
  // {{{ main
  $rootScope.$on('centralMenuReady', function (event, response)
  {
    factory.m_bCentralMenuReady = true;
    factory.ready();
  });
  $rootScope.$on('commonAuthReady', function (event, response)
  {
    factory.m_bCommonAuthReady = true;
    factory.ready();
  });
  $rootScope.$on('commonWsReady_Acorn', function (event, response)
  {
    factory.m_bCommonWsReady = true;
    factory.ready();
  });
  $rootScope.$on('footerReady', function (event, response)
  {
    factory.m_bFooterReady = true;
    factory.ready();
  });
  $http({method: 'GET', url: 'factory/settings.json'}).then(function (response)
  { 
    var strBridgeServer = 'localhost';
    if (angular.isDefined(response.data.bridgeServer))
    { 
      strBridgeServer = response.data.bridgeServer;
    }
    if (angular.isDefined(response.data.enableJwt))
    { 
      common.enableJwt(response.data.enableJwt);
    }
    if (angular.isDefined(response.data.redirectPath))
    { 
      common.setRedirectPath($location.protocol()+'://'+response.data.redirectPath);
    }
    if (angular.isDefined(response.data.secureLogin))
    { 
      common.setSecureLogin(response.data.secureLogin);
    }
    common.wsCreate('Acorn', 'bridge', strBridgeServer, '2797', (($location.protocol() === 'https')?true:false), 'bridge');
    $rootScope.$root.$broadcast('resetMenu', null);
  });
  // }}}
  return factory;
}
app.factory(factories);
