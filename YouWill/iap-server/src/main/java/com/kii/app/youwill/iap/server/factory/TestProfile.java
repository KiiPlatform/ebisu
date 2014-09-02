package com.kii.app.youwill.iap.server.factory;

import org.springframework.context.annotation.Profile;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Created by ethan on 14-8-18.
 */

@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
@Profile("test")
public @interface TestProfile {



}
