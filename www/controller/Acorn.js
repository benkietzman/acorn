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

var controllers = {};
// {{{ CentralMenu()
controllers.CentralMenu = function ($scope, central)
{
  $scope.central = central;
  $scope.store = {};
  central.setApplication('Acorn');
  central.centralMenu($scope.store);
};
// }}}
// {{{ footer()
controllers.footer = function ($http, $scope, acorn, common)
{
  $http({method: 'GET', url: 'factory/settings.json'}).then(function (response)
  {
    var strEngineer;
    if (angular.isDefined(response.data.engineer))
    {
      strEngineer = response.data.engineer;
    }
    common.setRequestPath('/central/include/request.php');
    common.request('footer', {userid: strEngineer}, function (result)
    {
      var error = {};
      $scope.$emit('footerReady', null);
      if (common.response(result, error))
      {
        $scope.footer = result.data.Response.out;
        $scope.footer.subject = 'Acorn';
        $scope.footer.version = '0.1';
      }
      else
      {
        $scope.message = error.message;
      }
    });
  });
};
// }}}
// {{{ About()
controllers.About = function ($http, $location, $scope, acorn, common)
{
  // {{{ variables
  $scope.controller = 'About';
  $scope.acorn = acorn;
  $scope.store = common.retrieve($scope.controller);
  // }}}
  // {{{ main
  common.setMenu($scope.controller);
  // }}}
};
// }}}
// {{{ Home()
controllers.Home = function ($http, $interval, $location, $scope, acorn, common)
{
  // {{{ variables
  $scope.controller = 'Home';
  $scope.acorn = acorn;
  $scope.store = common.retrieve($scope.controller);
  if (!angular.isDefined($scope.store.acorns))
  {
    $scope.store.acorns = [];
  }
  // }}}
  // {{{ getStatus()
  $scope.getStatus = function ()
  {
    if (acorn.ready())
    {
      var request = {Section: 'acorn', Request: {Acorn: 'router', 'Function': 'status'}};
      common.wsRequest('bridge', request).then(function (response)
      {
        var error = {};
        if (common.wsResponse(response, error))
        {
          $scope.store.routers = response.Response.Response;
          for (var i = 0; i < $scope.store.routers.length; i++)
          {
            $scope.store.routers[i].Acorns = [];
            $scope.store.routers[i].Buffers.Input /= 1024;
            $scope.store.routers[i].Buffers.Output /= 1024;
            if (angular.isDefined($scope.store.routers[i].Gateways))
            {
              for (var j = 0; j < $scope.store.routers[i].Gateways.length; j++)
              {
                $scope.store.routers[i].Gateways[j].background = (($scope.store.routers[i].Gateways[j].Enabled)?'#a9dfbf':(($scope.store.routers[i].Gateways[j].Connected)?'#fad7a0':'#f5b7b1'));
                $scope.store.routers[i].Gateways[j].Buffers.Input /= 1024;
                $scope.store.routers[i].Gateways[j].Buffers.Output /= 1024;
                if ($scope.store.routers[i].Gateways[i].Enabled)
                {
                  for (var k = 0; k < $scope.store.routers[i].Gateways[j].Acorns.length; k++)
                  {
                    var bFound = false;
                    for (var l = 0; !bFound && l < $scope.store.acorns.length; l++)
                    {
                      if ($scope.store.routers[i].Gateways[j].Acorns[k] == $scope.store.acorns[l])
                      {
                        bFound = true;
                      }
                    }
                    if (!bFound)
                    {
                      $scope.store.acorns.push($scope.store.routers[i].Gateways[j].Acorns[k]);
                    }
                    bFound = false;
                    for (var l = 0; !bFound && l < $scope.store.routers[i].Acorns.length; l++)
                    {
                      if ($scope.store.routers[i].Gateways[j].Acorns[k] == $scope.store.routers[i].Acorns[l])
                      {
                        bFound = true;
                      }
                    }
                    if (!bFound)
                    {
                      $scope.store.routers[i].Acorns.push($scope.store.routers[i].Gateways[j].Acorns[k]);
                    }
                  }
                }
              }
            }
          }
        }
        else
        {
          $scope.store.message = error.message.Message;
        }
      });
    }
  };
  // }}}
  // {{{ main
  common.setMenu($scope.controller);
  if (acorn.ready())
  { 
    $scope.getStatus();
  }
  $scope.$on('ready', function (event, response)
  {
    $scope.getStatus();
  });
  var getStatus = $interval(function()
  {
    $scope.getStatus();
  }, 5000);
  $scope.$on('$destroy', function ()
  {
    $interval.cancel(getStatus);
  });
  // }}}
};
// }}}
// {{{ menu()
controllers.menu = function ($http, $interval, $localStorage, $location, $scope, acorn, common)
{
  // {{{ variables
  $scope.application = 'Acorn';
  $scope.common = common;
  common.setApplication($scope.application);
  common.setRequestPath('/central/include/request.php');
  common.setSessionStorage($localStorage);
  // }}}
  // {{{ resetMenu()
  $scope.resetMenu = function ()
  {
    var unIndex, unSubIndex;
    common.clearMenu();
    unIndex = 0;
    common.m_menu.left[unIndex] = {value: 'Home', href: '/Home', icon: 'home', active: null};
    unIndex++;
    common.m_menu.left[unIndex] = {value: 'System', href: '/System', icon: 'cog', active: null};
    unIndex++;
    common.m_menu.left[unIndex] = {value: 'Try', href: '/Try', icon: 'share', active: null};
    unIndex++;
    unIndex = 0;
    common.m_menu.right[unIndex] = {value: 'About', href: '/About', icon: 'info-sign', active: null};
    unIndex++;
    common.auth($scope);
    common.setMenu(common.m_strMenu, common.m_strSubMenu);
  };
  // }}}
  // {{{ main
  $scope.$on('resetMenu', function (event, args)
  {
    $scope.resetMenu();
  });
  common.trackLocation();
  $scope.$on('$locationChangeStart', function(event)
  {
    common.trackLocation();
  });
  // }}}
};
// }}}
// {{{ System()
controllers.System = function ($http, $interval, $location, $scope, acorn, common)
{
  // {{{ variables
  $scope.controller = 'System';
  $scope.acorn = acorn;
  $scope.store = common.retrieve($scope.controller);
  // }}}
  // {{{ getSysInfo()
  $scope.getSysInfo = function ()
  {
    if (acorn.ready())
    {
      var request = {Section: 'central', 'Function': 'application'}
      request.Request = {name: 'Acorn'};
      common.wsRequest('bridge', request).then(function (response)
      {
        var error = {};
        if (common.wsResponse(response, error))
        {
          var request = {Section: 'central', 'Function': 'serverDetailsByApplicationID'};
          request.Request = {application_id: response.Response.id};
          common.wsRequest('bridge', request).then(function (response)
          {
            var error = {};
            if (common.wsResponse(response, error))
            {
              for (var i = 0; i < response.Response.length; i++)
              {
                if (response.Response[i].daemon)
                {
                  var request = {Section: 'sysInfo'};
                  request.Request = {Action: 'process', Server: response.Response[i].name, Process: response.Response[i].daemon, server_id: response.Response[i].server_id};
                  common.wsRequest('bridge', request).then(function (response)
                  {
                    var error = {};
                    if (common.wsResponse(response, error))
                    {
                      var nIndex = -1;
                      if (!$scope.store.sysInfo)
                      {
                        $scope.store.sysInfo = [];
                      }
                      for (var i = 0; i < $scope.store.sysInfo.length; i++)
                      {
                        if ($scope.store.sysInfo[i].ServerID == response.Request.server_id && $scope.store.sysInfo[i].Daemon == response.Request.Process)
                        {
                          nIndex = i;
                        }
                      }
                      if (nIndex == -1)
                      {
                        var info = {};
                        nIndex = $scope.store.sysInfo.length;
                        info.ServerID = response.Request.server_id;
                        info.Server = response.Request.Server;
                        info.Daemon = response.Request.Process;
                        $scope.store.sysInfo[nIndex] = info;
                      }
                      $scope.store.sysInfo[nIndex].data = response.Response;
                    }
                    else
                    {
                      $scope.store.message = error.message;
                    }
                  });
                }
              }
            }
            else
            {
              $scope.store.message = error.message;
            }
          });
        }
        else
        {
          $scope.store.message = error.message;
        }
      });
    }
  };
  // }}}
  // {{{ main
  common.setMenu($scope.controller);
  if (acorn.ready())
  { 
    $scope.getSysInfo();
  }
  $scope.$on('ready', function (event, response)
  {
    $scope.getSysInfo();
  });
  var getSysInfo = $interval(function()
  {
    $scope.getSysInfo();
  }, 10000);
  $scope.$on('$destroy', function ()
  {
    $interval.cancel(getSysInfo);
  });
  // }}}
};
// }}}
// {{{ Try()
controllers.Try = function ($http, $interval, $location, $scope, acorn, common)
{
  // {{{ variables
  $scope.controller = 'Try';
  $scope.acorn = acorn;
  $scope.store = common.retrieve($scope.controller);
  // }}}
  // {{{ change()
  $scope.change = function ()
  {
    if (angular.isDefined($scope.store.example.Request))
    {
      $scope.store.request = JSON.stringify($scope.store.example.Request);
    }
    else
    {
      $scope.store.request = null;
    }
  };
  // }}}
  // {{{ request()
  $scope.request = function ()
  {
    var request = {Section: 'acorn', Request: JSON.parse($scope.store.request)};
    if (!angular.isDefined(request.Request))
    {
      request.Request = {};
    }
    common.wsRequest('bridge', request).then(function (response)
    {
      $scope.store.response = response.Response;
    });
  };
  // }}}
  // {{{ main
  common.setMenu($scope.controller);
  if (acorn.ready())
  { 
  }
  $scope.$on('ready', function (event, response)
  {
  });
  if (!angular.isDefined($scope.store.examples))
  {
    $http({method: 'GET', url: 'factory/try.json'}).then(function (response)
    {
      $scope.store.examples = response.data;
      $scope.store.example = $scope.store.examples[0];
    });
  }
  // }}}
};
// }}}
app.controller(controllers);
