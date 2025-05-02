///////////////////////////////////////////
// Acorn
// -------------------------------------
// file       : app.js
// author     : Ben Kietzman
// begin      : 2018-12-27
// copyright  : Ben Kietzman
// email      : ben@kietzman.org
///////////////////////////////////////////
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
