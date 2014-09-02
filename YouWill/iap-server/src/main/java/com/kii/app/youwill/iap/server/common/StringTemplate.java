package com.kii.app.youwill.iap.server.common;

		import org.apache.commons.beanutils.BeanUtils;

import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class StringTemplate {

	private StringTemplate() {

	}

	public static final String generUrl(String template,
										Map<String, String> params) {
		String result = template;
		for (Map.Entry<String, String> entry : params.entrySet()) {
			String key = entry.getKey();

			String token = "\\$\\(" + key + "\\)";

			result = result.replaceAll(token, entry.getValue());
		}

		return result;
	}

	public static final String generByMap(String template, Map<String, String> params) {
		String result = template;
		for (Map.Entry<String, String> entry : params.entrySet()) {
			String key = entry.getKey();

			String token = "\\$\\{" + key + "\\}";

			result = result.replaceAll(token, entry.getValue());
		}

		return result;
	}

	public static final String gener(String template, String... params) {

		if(params==null ||  params.length==0){
			return template;
		}
		String result = template;
		for (int i = 0; i < params.length; i++) {
			String token = "\\$\\{" + i + "\\}";

			result = result.replaceAll(token, params[i]);
		}

		return result;
	}

	public static String generByObj(String template, Object entity) {
		Pattern exp=Pattern.compile("\\$\\{([^}]+)\\}");

		StringBuffer buf=new StringBuffer();
		Matcher match=exp.matcher(template);

		int start=0;
		while(match.find()){
			String field=match.group(1);

			int begin=match.start();
			buf.append(template.subSequence(start,begin));
			try {
				buf.append(BeanUtils.getProperty(entity, field));
			} catch (Exception e) {
				e.printStackTrace();
				buf.append(field);
			}
			start=match.end();
		}

		buf.append(template.substring(start));

		return buf.toString();
	}



}

