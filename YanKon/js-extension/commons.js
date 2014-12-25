MyKiiRequest = (function(){
	
	function MyKiiRequest(path){
	    this._path = "/apps/" + (Kii.getAppID()) +"/"+ path;
		
	    this._method = "GET";
	    this._headers = {
	      "accept": "*/*"
	    };
	    this._success = function(json, status) {
			console.log("success:"+json+" \n status:"+status);
		};
	    this._failure = function(errString, status) {
			console.log("failure:"+errString+" \n status:"+status);
    	
	    };
	
		var _this=this;
	    
	    this.onSuccess=function(anything, textStatus,jqXHR) {
			  
	        if ((200 <= (_ref1 = jqXHR.status) && _ref1 < 400)) {
	            if (jqXHR.status==204) {
	              return _this._success(null, textStatus);
	            } else {
					if (anything.errorCode != null) {
					  var errString=anything.errorCode+anything.message;
					  return _this._failure(errString, jqXHR.status, anything.errorCode);
	                } else {
	                  return _this._success(anything, textStatus);
	                }
	            }
	        } else {
	            var errString = xhr.status + " : " + _this._path;
	            var json = JSON.parse(decodeURIComponent(jqXHR.responseText));
	            if (json.errorCode != null) {
	                errString = json.errorCode+json.message;
	             }
	             return _this._failure(errString, jqXHR.status,json);
	       }
	    };
		
		this.onError=function(jqXHR,textStatus,errorThrown){
            var errString = textStatus + " : "  + _this._path;
            var json = JSON.parse(decodeURIComponent(jqXHR.responseText));
            if (json != null) {
              if (json.errorCode != null) {
                  errString = json.errorCode;
                if (json.message != null) {
                  errString += ": " + json.message;
                }
              }
            }
            return _this._failure(errString, jqXHR.status,json);
       };
	   
   	}
	
	MyKiiRequest.prototype.execute=function(callback){
		
		if(callback["success"]!=null){
			this._success=callback["success"];
		}
		if((callback["failure"]!=null)){
			this._failure=callback["failure"];
		}

	    var url = Kii.getBaseURL() + this._path;
		if(this._sub_path!=null){
			url+="/"+this._sub_path;
			delete this._sub_path;
		};
	
	
	    this._headers['x-kii-appid'] = Kii.getAppID();
	    this._headers['x-kii-appkey'] = Kii.getAppKey();
	    this._headers['x-kii-sdk'] = KiiSDKClientInfo.getSDKClientInfo();
	
	    if (this._accept != null) {
	      this._headers['accept'] = this._accept;
	    }
		if(this._token!=null){
  	      this._headers['Authorization'] = "Bearer " + this._token;
		  // delete this._token;
	    }
	
	    if (this._contentType != null) {
	      this._headers['Content-Type'] = this._contentType;
	    }else{
			this._headers['Content-Type']= "application/json";
	    }
		// delete this._contentType;
		
		var ajaxParam={};
		ajaxParam["method"]=this._method;
		ajaxParam["success"]=this.onSuccess;
		ajaxParam["error"]=this.onError;
		if(this._data!=null){
			ajaxParam["data"]=JSON.stringify(this._data);
		}
		ajaxParam["headers"]=this._headers;
	
		$.ajax(url,ajaxParam);
		
	}
	
	
	return MyKiiRequest;
})();
