package com.kii.app.youwill.iap.server.service;

import org.apache.commons.codec.binary.Hex;
import org.apache.commons.lang3.StringUtils;
import org.springframework.stereotype.Component;

import java.util.Date;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Created by ethan on 14-7-28.
 */
@Component
public class UUIDGeneral {

	private AtomicLong  seed=new AtomicLong(new Date().getTime());

	public String getUUID(String userID){

		long val=seed.decrementAndGet();

		String strVal=String.valueOf(val);
		strVal+=StringUtils.reverse(strVal);
		byte[] bytes= strVal.getBytes();

		byte[] mask = userID.getBytes();

		int length = Math.min(bytes.length, mask.length);

		byte[] result = new byte[length];
		for(int i=0;i<length;i++){
			byte res = (byte) (bytes[i] ^ mask[i]);

			result[i] = res;
		}

		return Hex.encodeHexString(result);

	}

}
