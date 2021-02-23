// vim600: fdm=marker
///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : app.js
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

var app = angular.module('app', ['ngCookies', 'ngRoute', 'ngStorage', 'ngWebSocket', 'ui.bootstrap']);
// {{{ routeProvider
app.config(function ($locationProvider, $routeProvider)
{
  $locationProvider.hashPrefix('');
  $routeProvider
  .when('/About', {templateUrl: 'template/About.html'})
  .when('/Home', {templateUrl: 'template/index.html'})
  .when('/System', {templateUrl: 'template/System.html'})
  .when('/Try', {templateUrl: 'template/Try.html'})
  .when('/Login', {templateUrl: '/include/common/angularjs/Login.html'})
  .when('/Logout', {templateUrl: '/include/common/angularjs/Logout.html'})
  .otherwise({redirectTo: '/Home'});
});
// }}}
