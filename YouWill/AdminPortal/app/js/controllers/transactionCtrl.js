/**
 * Created by leonardo on 21/08/2014.
 */
app.controller('transactionCtrl', function($scope, $rootScope, $modal, $location, $log) {
  $scope.init = function() {
    if($rootScope.logged != true) {
      $location.path("#/welcome");
    } else {
      $scope.loading = true;
      $scope.currentPage = 1;
      $scope.maxSize = 10;
      $scope.pageLimit = 10;
      $scope.loadTransaction();
      $scope.fields = [
        {name: "TransactionID", value: "transactionID", create: true, list: true},
        {name: "ProductID", value: "productID", create: true, list: true},
        {name: "PaymentID", value: "paymentID", create: true, list: true},
        {name: "Payment Method", value: "paymentMethod", create: true, list: true},
        {name: "Product Name", value: "productName", create: true, list: true},
        {name: "Transaction Creation Time", value: "transactionCreationTime", create: false, list: false, time: true},
        {name: "Transaction Update Time", value: "transactionUpdateTime", create: false, list: false, time: true},
        {name: "Payment Time", value: "paymentTime", create: false, list: false, time: true},
        {name: "Transaction Status", value: "transactionStatus", create: true, list: true, radio: true},
        {name: "Payment Status", value: "paymentStatus", create: true, list: true, radio: true}
      ]
    }
  }

  $scope.loadTransaction = function() {
    var bucket = Kii.bucketWithName("transaction");
    var query = KiiQuery.queryWithClause();
    query.setLimit($scope.pageLimit);
    query.sortByDesc("_modified");
    var transactions = [];
    $scope.transactions = [];

    bucket.count({
      success: function(bucket, query, count) {
        console.log("Number of objects : " + count);
        $scope.totalItems = count;
        $scope.$apply();
      },
      failure: function(bucket, query, errorString) {
        console.log("Execution failed by : " + errorString);
      }})

    var queryCallbacks = {
      success: function(queryPerformed, resultSet, nextQuery) {
        if (resultSet != 0) {
          var transactionsSinglePage = [];
          for (var i = 0; i < resultSet.length; i++) {
            transactionsSinglePage.push(resultSet[i]);
          }
          transactions.push(transactionsSinglePage);
          $scope.transactions.push(transactionsSinglePage);
          $scope.loading = false;
          $scope.$apply();
        } else {
          $scope.loading = false;
          $scope.$apply();
        }
        if(nextQuery != null) {
          console.log('has next');
          bucket.executeQuery(nextQuery, queryCallbacks);
        } else if(nextQuery == null) {
          console.log(transactions)
        }
      },
      failure: function(queryPerformed, anErrorString) {
      }
    }
    bucket.executeQuery(query, queryCallbacks);
  }

  $scope.openModal = function(transaction) {
    var modalInstance = $modal.open({
      templateUrl: './partials/transactionCreationModal.html',
      controller: ModalInstanceCtrl,
      size: "lg",
      backdrop: 'static',
      resolve: transaction ? {
        transaction: function(){
          return transaction
        },
        fields: function(){
          return $scope.fields
        }
      } : {
        transaction: function(){
          return undefined
        },
        fields: function(){
          return $scope.fields
        }
      }
    });

    modalInstance.result.then(function (selectedItem) {
      console.log("modal closed and returned to transaction")
      $scope.loadTransaction();
      $scope.loading = true;
    }, function () {
      $log.info('Modal dismissed at: ' + new Date());
    });
  }
  var ModalInstanceCtrl = function ($scope, $modalInstance, transaction, fields) {
    $scope.init = function() {
      if (transaction){
        console.log('see you again')
        $scope.transaction = transaction._customInfo;
        $scope.transactionCopy = JSON.parse(JSON.stringify(transaction._customInfo))
        $scope.virgin = false;
        $scope.title = "Transaction Info";
        $scope.fields = fields;
        console.log(transaction)
      } else {
        console.log("transaction creation")
        $scope.virgin = true;
        $scope.transaction = {
          transactionStatus: 'pending',
          paymentStatus: 'pending'
        };
        $scope.title = "New Transaction";
        $scope.fields = fields;
      }
      $scope.btnLoading = false;
    }
    $scope.cancel = function () {
      $modalInstance.dismiss('cancel');
    };

    $scope.create = function() {
      console.log($scope.transaction)
      $scope.btnLoading = true;
      var appBucket = Kii.bucketWithName("transaction");
      var obj = ($scope.virgin == true) ? appBucket.createObject() : transaction;
      $scope.fields.map(function(field){
        if (field.create){
          eval("obj.set('" + field.value + "', $scope.transaction." + field.value + ")")
        }
      })
      if ($scope.virgin){
        obj.set("transactionCreationTime", new Date() / 1000);
      }
      obj.set("transactionUpdateTime", new Date() / 1000);
      if ($scope.transaction.paymentStatus == "completed" && $scope.transactionCopy.paymentStatus == "pending") {
        console.log("payment status modified")
        obj.set("paymentTime", new Date() / 1000);
      }
      obj.save({
        success: function(theObject) {
          console.log("Object saved!");
          console.log(theObject);
          $scope.btnLoading = false;
          $modalInstance.close();
        },
        failure: function(theObject, errorString) {
          console.log("Error saving object: " + errorString);
        }
      });
    }
  }
})