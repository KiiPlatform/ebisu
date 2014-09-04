/**
 * Created by leonardo on 21/08/2014.
 */
app.controller('configCtrl', function($scope, $rootScope, $location) {

  $scope.init = function() {
    if($rootScope.logged != true) {
      $location.path("#/welcome");
    } else {
      $scope.virgin = undefined;
      $scope.pageLoading = true;
      $scope.loading = false;
      $scope.noticeShow = false;
      $scope.configurePrev = {};
      $scope.loadConfig();
    }
  }

  $scope.setObj = function(obj){
    obj.set("paypalClientID", $scope.configure.paypalClientID);
    obj.set("paypalSecret", $scope.configure.paypalSecret);
    obj.set("paypalSandbox", $scope.configure.paypalSandbox);
    obj.set("paypalSandboxSecret", $scope.configure.paypalSandboxSecret);
    obj.set("alipayPartnerID", $scope.configure.alipayPartnerID);
    obj.set("alipayRSAPrivateKey", $scope.configure.alipayRSAPrivateKey);
    obj.set("alipaySecurityKey", $scope.configure.alipaySecurityKey);
    obj.set("iapSecurityKey", $scope.configure.iapSecurityKey);
    obj.save({
      success: function(theObject) {
        $scope.loading = false;
        $scope.noticeShow = true;
        $scope.$apply();
        console.log("Object saved!");
        console.log(theObject);
      },
      failure: function(theObject, errorString) {
        console.log("Error saving object: " + errorString);
      }
    });
  }

  $scope.saveConfig = function() {
    $scope.loading = true;
    $scope.noticeShow = false;
    if ($scope.virgin == true) {
      var appBucket = Kii.bucketWithName("configure");
      var obj = appBucket.createObject();
      $scope.setObj(obj);
    } else if ($scope.virgin == false) {
      var obj = KiiObject.objectWithURI($scope.currentConfigURI);
      $scope.setObj(obj);
    }
  }

  $scope.loadConfig = function() {
    var bucket = Kii.bucketWithName("configure");
    var query = KiiQuery.queryWithClause();

    var queryCallbacks = {
      success: function(queryPerformed, resultSet, nextQuery) {
        $scope.pageLoading = false;
        if (resultSet.length == 0) {
          console.log('virgin user');
          $scope.configure = {};
          $scope.virgin = true;
          $scope.$apply();
        } else {
          console.log('whoremaster coming with ' + resultSet.length + ' configs');
          $scope.currentConfigURI = resultSet[0].objectURI();
          console.log(resultSet[0]._customInfo)
          $scope.virgin = false;
          var configure = {
            paypalClientID: resultSet[0].get('paypalClientID'),
            paypalSecret: resultSet[0].get('paypalSecret'),
            paypalSandbox: resultSet[0].get('paypalSandbox'),
            paypalSandboxSecret: resultSet[0].get('paypalSandboxSecret'),
            alipayPartnerID: resultSet[0].get('alipayPartnerID'),
            alipayRSAPrivateKey: resultSet[0].get('alipayRSAPrivateKey'),
            alipaySecurityKey: resultSet[0].get('alipaySecurityKey'),
            iapSecurityKey: resultSet[0].get('iapSecurityKey')
          };
          $scope.configure = configure;
          $scope.configurePrev = JSON.parse(JSON.stringify(configure));
          $scope.$apply();
        }
        if(nextQuery != null) {
          bucket.executeQuery(nextQuery, queryCallbacks);
        }
      },
      failure: function(queryPerformed, anErrorString) {
        console.log('error querying config data', anErrorString)
      }
    }
    bucket.executeQuery(query, queryCallbacks);
  }

  $scope.restoreConfig = function() {
    console.log('config restored to previous')
    $scope.configure = $scope.configurePrev;
  }
})