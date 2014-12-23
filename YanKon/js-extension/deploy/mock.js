
MyRequest = (function(){
	
	function MyRequest(path){
	    this._path = "/apps/" + (Kii.getAppID()) +"/"+ path;
		
	    this._method = "GET";
	    this._headers = {
	      "accept": "*/*"
	    };
	    this._success = function(json, status) {
			recordLog("kii request","success:"+json+" \n status:"+status);
		};
	    this._failure = function(errString, status) {
			recordLog("kii request","failure:"+errString+" \n status:"+status);
    	
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
	             return _this._failure(errString, textStatus);
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
            return _this._failure(errString, textStatus);
       };
	   
   	}
	
	MyRequest.prototype.execute=function(callback){
		
		if(callback!=null){
			if(callback["success"]!=null){
				this._success=callback["success"];
			}
			if((callback["failure"]!=null)){
				this._failure=callback["failure"];
			}
		}

	    var url = Kii.getBaseURL() + this._path;
		if(this._sub_path!=null){
			url+="/"+this._sub_path;
			delete this._sub_path;
		};
	
		var json_text=null;
		if(this._data!=null){
		    json_text = JSON.stringify(this._data);
		}
		this._data=null;
		
	    this._headers['x-kii-appid'] = Kii.getAppID();
	    this._headers['x-kii-appkey'] = Kii.getAppKey();
	    this._headers['x-kii-sdk'] = KiiSDKClientInfo.getSDKClientInfo();
	
	    if (this._accept != null) {
	      this._headers['accept'] = this._accept;
	    }
		if(this._token!=null){
  	      this._headers['Authorization'] = "Bearer " + this._token;
		  delete this._token;
	    }
	
	    if (this._contentType != null) {
	      this._headers['Content-Type'] = this._contentType;
	    }else{
			this._headers['Content-Type']= "application/json";
	    }
		delete this._contentType;
		
		var ajaxParam={};
		ajaxParam["contextType"]=this._contextType;
		ajaxParam["method"]=this._method;
		ajaxParam["success"]=this.onSuccess;
		ajaxParam["error"]=this.onError;
		if(json_text!=null){
			ajaxParam["data"]=json_text;
		}
		ajaxParam["headers"]=this._headers;
	
		$.ajax(url,ajaxParam);
		
	}
	return MyRequest;
})();


Context= (function(){
	
	function Context(appID,appKey,url,clientID,secret,callback){
		
		this.appID=appID;
		this.appKey=appKey;
		
		Kii.initializeWithSite(appID, appKey, url);
		
		var _this=this;
		Kii.authenticateAsAppAdmin(clientID,clientSecret, {
		    success: function(adminContext) {
				_this.adminCtx=adminContext;
				console.log("finish init");
				callback();
			}
		});
	}
	
	Context.prototype.setToken=function(user){
		this.token=user.getAccessToken();
	}

	
	Context.prototype.getAccessToken=function(){
		return this.token;
	}
	
	Context.prototype.getAppAdminContext=function(){
		return this.adminCtx;
	}
	
	Context.prototype.getAppID=function(){
		return this.appID;
	}
	
	Context.prototype.getAppKey=function(){
		return this.appKey;
	}
	
	Context.prototype.setUserLogin=function(userName,pwd){
		this.userName=userName;
		this.pwd=pwd;
	}
	
	Context.prototype.setThingLogin=function(thing,pwd){
		this.thing=thing;
		this.pwd=pwd;
	}
	
	Context.prototype.fireFun=function(funName,param,fun){
		
		if(fun==null){
			fun=function(param){
				console.log("return:"+JSON.stringify(param));
			}
		}
		
		var callback= function(result){
			console.log("return result:"+result);
			if(this.remote!=null){
				var returnVal=response["returnedValue"];
				fun(returnVal);
			}else{
				fun(result);
			}
		};
		
			
		if(this.userName==null){
			var req=new MyRequest("oauth2/token");
			req._method="POST";
			req._contentType="application/vnd.kii.OauthTokenRequest+json";
			var data={};
			data["username"]="VENDOR_THING_ID:"+this.thing;
			data["password"]=this.pwd;
			data["grant_type"]="password";
			req._data=data;
			
			var _this=this;
			req.execute({
				
				success:function(result){
					_this.token=result["access_token"];
					_this.fireRemoteFun(funName,param,fun);
				}
			})
			
		}else{
			var _this=this;
			KiiUser.authenticate(this.userName, this.pwd, {
				success:function(user){
					_this.setToken(user);
					_this.fireRemoteFun(funName,param,fun);
				}
			});
		}
		return;
		
	}
	
	Context.prototype.fireRemoteFun=function(funName,param,callback){
		
		
		if(this.remote==null){
			eval(funName)(param,this,callback);
			return;
		}
		
		var request=new MyRequest("server-code/versions/current");
		
		request._sub_path=funName;
		request._data=param;
		request._token=this.token;
		request._method="POST";

		request.execute({success:callback});
		
	}
	
	return Context;
	
})();

