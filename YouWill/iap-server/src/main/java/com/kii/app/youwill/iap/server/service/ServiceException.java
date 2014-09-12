package com.kii.app.youwill.iap.server.service;

import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.http.HttpStatus;

/**
 * Created by ethan on 14-7-28.
 */
public class ServiceException extends RuntimeException{

	private IAPErrorCode errorCode;

	private String errorMsg;

	public ServiceException(IAPErrorCode errorCode){
		this.errorCode=errorCode;
	}

	public ServiceException(IAPErrorCode errorCode,String errorMsg){
		this.errorCode=errorCode;
		this.errorMsg=errorMsg;
	}

	public HttpStatus getHttpStatus(){
        try {
            return HttpStatus.valueOf(errorCode.code);
        } catch (Exception e) {
            return HttpStatus.INTERNAL_SERVER_ERROR;
        }
	}

	public JSONObject getErrorCode(){

		try {
			JSONObject obj = new JSONObject();

			obj.put("errorCode", errorCode.getCode());
			obj.put("msg", errorCode.name());


			return obj;
		}catch (JSONException e){
			throw new IllegalArgumentException(e);
		}
	}


}
