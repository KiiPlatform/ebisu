package com.kii.app.youwill.iap.server.web;

import com.kii.app.youwill.iap.server.service.ServiceException;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ControllerAdvice;
import org.springframework.web.bind.annotation.ExceptionHandler;

/**
 * Created by ethan on 14-8-5.
 */

@ControllerAdvice
public class IAPErrorController {

	@ExceptionHandler(ServiceException.class)
	public ResponseEntity<String> handleServiceException(ServiceException ex) {


		String error=ex.getErrorCode().toString();

		ResponseEntity<String> resp=new ResponseEntity(error,ex.getHttpStatus());
		return resp;
	}


}
