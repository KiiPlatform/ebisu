package com.kii.app.youwill.iap.server.dao;

import com.kii.platform.ufp.errors.ErrorCode;
import com.kii.platform.ufp.errors.KiiException;

/**
 * Created by ethan on 14-8-1.
 */
public class KiiRuntimeException extends RuntimeException {

	private ErrorCode error;

	public KiiRuntimeException(KiiException excep) {
		error = excep.getErrorCode();
	}

	public ErrorCode getError() {
		return error;
	}


}
