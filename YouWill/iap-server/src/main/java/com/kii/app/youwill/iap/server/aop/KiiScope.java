package com.kii.app.youwill.iap.server.aop;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Created by ethan on 14-8-4.
 */
@Target({ElementType.TYPE,ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
public @interface KiiScope {

	ScopeType scope() default  ScopeType.CurrUser;

	ScopeType right() default ScopeType.CurrUser;

	String username() default "";

}
