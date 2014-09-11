package com.kii.app.youwill.iap.server.aop;

import com.kii.app.youwill.iap.server.web.AppContext;
import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.aspectj.lang.reflect.MethodSignature;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;

import javax.annotation.PostConstruct;
import java.lang.reflect.Method;

/**
 * Created by ethan on 14-8-4.
 */
@Aspect
public class KiiScopeManagerAdape  {

	private Logger log= LoggerFactory.getLogger(KiiScopeManagerAdape.class);

	@Autowired
	private AppContext appContext;

	@PostConstruct
	public void init(){


	}

	private Object doScopeOper(ProceedingJoinPoint joinPoint) throws Throwable {

		MethodSignature signature = (MethodSignature) joinPoint.getSignature();
		Method method = signature.getMethod();

		Object result=null;

		KiiScope scope=method.getAnnotation(KiiScope.class);
		if(scope==null){

			scope=(KiiScope)joinPoint.getTarget().getClass().getAnnotation(KiiScope.class);
		}

			switch (scope.scope()) {

				case Admin:
					appContext.asApp();
					break;
				case CurrUser:
					break;
//			case SpecUser:
//				appContext.sudo(scope.username());

			}

		switch(scope.right()){
			case Admin:
				appContext.su();
				break;
			case CurrUser:
				break;

		}

		result = joinPoint.proceed();


		if(scope.scope()!= ScopeType.CurrUser){
			appContext.exitScope();
		}
		if(scope.right()!=ScopeType.CurrUser){
			appContext.exit();
		}

		return result;

	}

	@Around("execution(* com.kii.app.youwill.iap.server.dao.*.*(..))  &&  " +
					"@annotation(KiiScope)" )
	public Object withScopeMethod(ProceedingJoinPoint joinPoint) throws Throwable {

		return doScopeOper(joinPoint);


	}




	@Around("execution(* com.kii.app.youwill.iap.server.dao.*.*(..)) && " +
			"@within(KiiScope)" )
	public Object  withScopeClass(ProceedingJoinPoint joinPoint) throws Throwable {

		return doScopeOper(joinPoint);




	}


}
