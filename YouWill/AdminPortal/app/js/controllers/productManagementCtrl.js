/**
 * Created by leonardo on 21/08/2014.
 */
app.controller('productManagementCtrl', function ($scope, $rootScope, $location, $modal, $log) {
  $scope.init = function () {
    if ($rootScope.logged != true) {
      $location.path("#/welcome");
    } else {
      $scope.loading = true;
      $scope.currentPage = 1;
      $scope.maxSize = 10;
      $scope.pageLimit = 10;
      $scope.loadProduct();
      $scope.fields = [
        {name: "Product ID", value: "productID", list: true},
        {name: "Name", value: "name", list: true},
        {name: "Category", value: "category", list: true},
        {name: "Description", value: "description", list: false},
        {name: "Price", value: "price", list: true},
        {name: "New", value: "isNew", list: false},
        {name: "Valid", value: "valid", list: false},
        {name: "Consume Type", value: "consumeType", list: true},
        {name: "App ID", value: "appID", list: true},
        {name: "Version", value: "version", list: true},
        {name: "Version Code", value: "versionCode", list: false},
        {name: "Author", value: "author", list: true},
        {name: "Rate", value: "rate", list: false}
      ]
    }
  }

  $scope.loadProduct = function () {
    var bucket = Kii.bucketWithName("product");
    var query = KiiQuery.queryWithClause();
    query.setLimit($scope.pageLimit);
    query.sortByDesc("_modified");
    var products = [];
    $scope.products = [];

    bucket.count({
      success: function (bucket, query, count) {
        console.log("Number of objects : " + count);
        $scope.totalItems = count;
        $scope.$apply();
      },
      failure: function (bucket, query, errorString) {
        console.log("Execution failed by : " + errorString);
      }})

    var queryCallbacks = {
      success: function (queryPerformed, resultSet, nextQuery) {
        if (resultSet != 0) {
          var productsSinglePage = [];
          for (var i = 0; i < resultSet.length; i++) {
            productsSinglePage.push(resultSet[i]);
          }
          products.push(productsSinglePage);
          $scope.products.push(productsSinglePage);
          $scope.loading = false;
          $scope.$apply();
        } else {
          console.log("no product yet!");
          $scope.loading = false;
          $scope.$apply();
        }
        if (nextQuery != null) {
          console.log('has next');
          bucket.executeQuery(nextQuery, queryCallbacks);
        } else if (nextQuery == null) {
          console.log(products)
        }
      },
      failure: function (queryPerformed, anErrorString) {
      }
    }
    bucket.executeQuery(query, queryCallbacks);
  }

  $scope.openModal = function (product) {
    var modalInstance = $modal.open({
      templateUrl: './partials/modalContent.html',
      controller: ModalInstanceCtrl,
      size: "lg",
      backdrop: 'static',
      resolve: product ? {
        product: function () {
          return product
        },
        fields: function(){
          return $scope.fields
        }
      } : {
        product: function () {
          return undefined
        },
        fields: function(){
          return $scope.fields
        }
      }
    });

    modalInstance.result.then(function (selectedItem) {
      console.log("modal closed and returned to product")
      $scope.loadProduct();
      $scope.loading = true;
      $scope.selected = selectedItem;
    }, function () {
      $log.info('Modal dismissed at: ' + new Date());
    });
  };

  var ModalInstanceCtrl = function ($scope, $modalInstance, product, fields) {
    $scope.init = function () {
      if (product) {
        console.log('see you again')
        $scope.imgLoading = true;
        $scope.product = product._customInfo;
        $scope.virgin = false;
        $scope.title = "Product Info";
        console.log(product)
        $scope.loadProductImg();
      } else {
        $scope.virgin = true;
        $scope.product = {
          isNew: false,
          valid: false,
          consumeType: "Consumable"
        };
        $scope.title = "New Product";
      }
      $scope.uploader = {};
      $scope.fields = fields;
      $scope.alertShow = false
      $scope.btnLoading = false;
    }

    $scope.loadProductImg = function () {
      product.publishBody({
        success: function (obj, publishedUrl) {
          $scope.productImgURL = publishedUrl;
          console.log("load img done: ", publishedUrl)
          $scope.$apply();
        },
        failure: function (obj, anErrorString) {
          console.log(anErrorString);
          $scope.productImgURL = "http://www.placehold.it/200x150/EFEFEF/AAAAAA&text=no+image";
          $scope.$apply();
        }
      });
    }

    $scope.uploadBody = function (theObject) {
      var srcData = new Blob([$scope.uploader.flow.files[0].file], {type: $scope.uploader.flow.files[0].file.type});
      theObject.uploadBody(srcData, {
        success: function (obj) {
          console.log("Object and body saved!");
          console.log(theObject);
          $scope.btnLoading = false;
          $modalInstance.close();
        },
        failure: function (obj, anErrorString) {
          console.log("Error saving object: " + errorString);
        }
      })
    }
    $scope.create = function () {
      console.log(Object.keys($scope.product).length)
      if (Object.keys($scope.product).length == 13) {
        console.log($scope.product)
        $scope.btnLoading = true;
        var appBucket = Kii.bucketWithName("product");
        var obj = ($scope.virgin == true) ? appBucket.createObject() : product;
//        obj.set("productID", $scope.product.productID);
//        obj.set("name", $scope.product.name);
//        obj.set("description", $scope.product.description);
//        obj.set("category", $scope.product.category);
//        obj.set("price", $scope.product.price);
//        obj.set("new", $scope.product.new);
//        obj.set("recommended", $scope.product.recommended);
//        obj.set("consumable", $scope.product.consumable);
//        obj.set("valid", $scope.product.valid);
        $scope.fields.map(function (field) {
          eval("obj.set('" + field.value + "', $scope.product." + field.value + ")")
        })

        obj.save({
          success: function (theObject) {
            if ($scope.uploader.flow.files.length) {
              $scope.uploadBody(theObject);
            } else {
              console.log("Object saved!");
              console.log(theObject);
              $scope.btnLoading = false;
              $modalInstance.close();
            }
          },
          failure: function (theObject, errorString) {
            console.log("Error saving object: " + errorString);
          }
        });
      } else {
        $scope.alertShow = true;
      }
    };

    $scope.cancel = function () {
      $modalInstance.dismiss('cancel');
    };
  };
})