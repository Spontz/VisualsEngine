/*
	exprevaldriver.c: driver to expreval library
*/

#include <float.h>
#include <stdio.h>

#include "exprevaldriver.h"
#include "gldriver.h"
#include "../dkernel.h"

int removeSpaces(char* Str)	{
	unsigned int i = 0;
	unsigned int p = 0;

	if (Str==0)
		return 0;

	for (; i < strlen(Str); ++i)
		if (Str[i] != ' ')
			Str[p++] = Str[i];
	Str[p] = '\0';

	return i-p;
}

EXPRTYPE saturate(EXPRTYPE in) {
	return (in < 0) ? 0 : ( (in > 1) ? 1 : in );
}

EXPR_FUNCTIONSOLVER(expr_smoothstep) {
	int err;
	EXPRTYPE v0, v1, t, x;

	EXPR_REQUIRECOUNT(3);

	EXPR_EVALNODE(0, v0);
	EXPR_EVALNODE(1, v1);
	EXPR_EVALNODE(2, t);

	if (fabs(v1 - v0) < FLT_EPSILON)
		EXPR_RETURNSOFTERR(EXPR_ERROR_OUTOFRANGE);

	EXPR_CLEARMATHERR();

	x = saturate((t - v0) / (v1 - v0)); // the division by 0 has been already controlled
	*val = x*x*(3 - 2*x);

	EXPR_CHECKMATHERR();

	return EXPR_ERROR_NOERROR;
}

EXPR_FUNCTIONSOLVER(expr_hermite_interpolation) {
	int err;
	EXPRTYPE v0, v1, t;

	EXPR_REQUIRECOUNT(3);

	EXPR_EVALNODE(0, v0);
	EXPR_EVALNODE(1, v1);
	EXPR_EVALNODE(2, t);

	if (fabs(v1 - v0) < FLT_EPSILON)
		EXPR_RETURNSOFTERR(EXPR_ERROR_OUTOFRANGE);

	EXPR_CLEARMATHERR();

	*val = v0 + t*t*(3 - 2*t)*(v1-v0);

	EXPR_CHECKMATHERR();

	return EXPR_ERROR_NOERROR;
}

void initExpression(tExpression *myExpression) {
	static int	count = 0;

	int			removedSpacesCount;
	char		num[1024];

	myExpression->pOwner = mySection;

	removedSpacesCount = removeSpaces(myExpression->equation);
	
	dkernel_trace("exprevaldriver: initExpression: [%s] (%d spaces removed)", myExpression->equation, removedSpacesCount);
	
	sprintf(num, "%d", count++);
	//if (count>141)
	//MessageBox(0, myExpression->equation, num, 0);

	myExpression->err = exprFuncListCreate(&myExpression->f);
	if(myExpression->err != EXPR_ERROR_NOERROR) {
		dkernel_error("Function List Creation Error %d\n", myExpression->err);
		return;
	}
	
	/* Initialize internal functions */
	myExpression->err = exprFuncListInit(myExpression->f);
	if(myExpression->err != EXPR_ERROR_NOERROR) {
		dkernel_error("Function List Initialization Error %d\n", myExpression->err);
		dkernel_error("Some internal functions may not be usable\n");
		return;
	}
	
	/* Create variable list */
	myExpression->err = exprValListCreate(&myExpression->v);
	if(myExpression->err != EXPR_ERROR_NOERROR) {
		dkernel_error("Variable List Creation Error %d\n", myExpression->err);
		exprFuncListFree(myExpression->f);
		return;
	}
	
	/* Create the constant list */
	myExpression->err = exprValListCreate(&myExpression->c);
	if(myExpression->err != EXPR_ERROR_NOERROR) {
		dkernel_error("Constant List Creation Error %d\n", myExpression->err);
		exprFuncListFree(myExpression->f);
		exprValListFree(myExpression->v);
		return;
	}
	
	/* Initialize internal constants */
	myExpression->err = exprValListInit(myExpression->c);
	if(myExpression->err != EXPR_ERROR_NOERROR) {
		dkernel_error("Constant List Initialization Error %d\n", myExpression->err);
		dkernel_error("Some internal constants may not be usable\n");
		return;
	}
	
	/* Add any application defined functions, constants, or variables to the lists here or down below */
	
	myExpression->err = exprCreate(&myExpression->o, myExpression->f, myExpression->v, myExpression->c, NULL, 0, NULL);
	if(myExpression->err != EXPR_ERROR_NOERROR) {
		dkernel_error("Expression Object Creation Error %d\n", myExpression->err);
		exprFuncListFree(myExpression->f); //-V525
		exprValListFree(myExpression->c);
		exprValListFree(myExpression->v);
		return;
	}
	
	/* Add any application defined functions, constants, or variables to the lists here or down below.
		This is the last time you can for the functions or constants. */


	// Add custom functions
	exprFuncListAdd(myExpression->f, expr_smoothstep, "ss", 3, 3, 0, 0);
	exprFuncListAdd(myExpression->f, expr_hermite_interpolation, "hi", 3, 3, 0, 0);

	/* Functions and constants may be added or changed here */
	
	myExpression->err = exprParse(myExpression->o, myExpression->equation);
	if(myExpression->err != EXPR_ERROR_NOERROR) {
		dkernel_error (
			"%s : Error %d parsing section %s expression (layer:%d, start:%.5g, end:%.5g)\n\n%s",
			myExpression->pOwner->DataSource ? myExpression->pOwner->DataSource : "Unknown data source",
			myExpression->err,
			myExpression->pOwner->identifier,
			myExpression->pOwner->layer,
			myExpression->pOwner->startTime,
			myExpression->pOwner->endTime,
			myExpression->equation ? myExpression->equation : "Equation String is NULL"
			);

		/* Free objects and return */
		exprFree(myExpression->o);
		
		/* Free lists */
	}
}

void insertSectionVariables(tExpression *expression) {
	// These are the variables that can be used in all formulas and must be updated on each frame
	exprValListAdd(expression->v ,    "t", (EXPRTYPE)mySection->runTime );
	exprValListAdd(expression->v , "tend", (EXPRTYPE)mySection->duration);
	exprValListAdd(expression->v , "beat", (EXPRTYPE)demoSystem.beat    );
	exprValListAdd(expression->v ,  "fps", (EXPRTYPE)demoSystem.fps     );

	// Camera values
	if (demoSystem.camera != NULL) {
		exprValListAdd(expression->v , "cam_eye_x", (EXPRTYPE)demoSystem.camera->eye.x);
		exprValListAdd(expression->v , "cam_eye_y", (EXPRTYPE)demoSystem.camera->eye.y);
		exprValListAdd(expression->v , "cam_eye_z", (EXPRTYPE)demoSystem.camera->eye.z);
		
		exprValListAdd(expression->v , "cam_target_x", (EXPRTYPE)demoSystem.camera->target.x);
		exprValListAdd(expression->v , "cam_target_y", (EXPRTYPE)demoSystem.camera->target.y);
		exprValListAdd(expression->v , "cam_target_z", (EXPRTYPE)demoSystem.camera->target.z);

		exprValListAdd(expression->v , "cam_angle_x", (EXPRTYPE)demoSystem.camera->angle.x);
		exprValListAdd(expression->v , "cam_angle_y", (EXPRTYPE)demoSystem.camera->angle.y);
		exprValListAdd(expression->v , "cam_angle_z", (EXPRTYPE)demoSystem.camera->angle.z);
	} else {
		exprValListAdd(expression->v , "cam_eye_x", 0);
		exprValListAdd(expression->v , "cam_eye_y", 0);
		exprValListAdd(expression->v , "cam_eye_z", 0);

		exprValListAdd(expression->v , "cam_target_x", 0);
		exprValListAdd(expression->v , "cam_target_y", 0);
		exprValListAdd(expression->v , "cam_target_z", 100);

		exprValListAdd(expression->v , "cam_angle_x", 0);
		exprValListAdd(expression->v , "cam_angle_y", 0);
		exprValListAdd(expression->v , "cam_angle_z", 0);
	}
	
	// Graphic constants
	exprValListAdd(expression->v, "vpWidth", (EXPRTYPE)glDriver.vpWidth);
	exprValListAdd(expression->v, "vpHeight", (EXPRTYPE)glDriver.vpHeight);

	exprValListAdd(expression->v, "aspectRatio", (EXPRTYPE)gldrv_get_viewport_aspect_ratio());

	// Fbo constants
	exprValListAdd(expression->v, "fbo0Width", (EXPRTYPE)glDriver.fbo[0].width);
	exprValListAdd(expression->v, "fbo0Height", (EXPRTYPE)glDriver.fbo[0].height);
	exprValListAdd(expression->v, "fbo1Width", (EXPRTYPE)glDriver.fbo[1].width);
	exprValListAdd(expression->v, "fbo1Height", (EXPRTYPE)glDriver.fbo[1].height);
	exprValListAdd(expression->v, "fbo2Width", (EXPRTYPE)glDriver.fbo[2].width);
	exprValListAdd(expression->v, "fbo2Height", (EXPRTYPE)glDriver.fbo[2].height);
	exprValListAdd(expression->v, "fbo3Width", (EXPRTYPE)glDriver.fbo[3].width);
	exprValListAdd(expression->v, "fbo3Height", (EXPRTYPE)glDriver.fbo[3].height);
	exprValListAdd(expression->v, "fbo4Width", (EXPRTYPE)glDriver.fbo[4].width);
	exprValListAdd(expression->v, "fbo4Height", (EXPRTYPE)glDriver.fbo[4].height);

	exprValListAdd(expression->v, "fbo5Width", (EXPRTYPE)glDriver.fbo[5].width);
	exprValListAdd(expression->v, "fbo5Height", (EXPRTYPE)glDriver.fbo[5].height);
	exprValListAdd(expression->v, "fbo6Width", (EXPRTYPE)glDriver.fbo[6].width);
	exprValListAdd(expression->v, "fbo6Height", (EXPRTYPE)glDriver.fbo[6].height);
	exprValListAdd(expression->v, "fbo7Width", (EXPRTYPE)glDriver.fbo[7].width);
	exprValListAdd(expression->v, "fbo7Height", (EXPRTYPE)glDriver.fbo[7].height);
	exprValListAdd(expression->v, "fbo8Width", (EXPRTYPE)glDriver.fbo[8].width);
	exprValListAdd(expression->v, "fbo8Height", (EXPRTYPE)glDriver.fbo[8].height);
	exprValListAdd(expression->v, "fbo9Width", (EXPRTYPE)glDriver.fbo[9].width);
	exprValListAdd(expression->v, "fbo9Height", (EXPRTYPE)glDriver.fbo[9].height);

	exprValListAdd(expression->v, "fbo10Width", (EXPRTYPE)glDriver.fbo[10].width);
	exprValListAdd(expression->v, "fbo10Height", (EXPRTYPE)glDriver.fbo[10].height);
	exprValListAdd(expression->v, "fbo11Width", (EXPRTYPE)glDriver.fbo[11].width);
	exprValListAdd(expression->v, "fbo11Height", (EXPRTYPE)glDriver.fbo[11].height);
	exprValListAdd(expression->v, "fbo12Width", (EXPRTYPE)glDriver.fbo[12].width);
	exprValListAdd(expression->v, "fbo12Height", (EXPRTYPE)glDriver.fbo[12].height);
	exprValListAdd(expression->v, "fbo13Width", (EXPRTYPE)glDriver.fbo[13].width);
	exprValListAdd(expression->v, "fbo13Height", (EXPRTYPE)glDriver.fbo[13].height);
	exprValListAdd(expression->v, "fbo14Width", (EXPRTYPE)glDriver.fbo[14].width);
	exprValListAdd(expression->v, "fbo14Height", (EXPRTYPE)glDriver.fbo[14].height);

	exprValListAdd(expression->v, "fbo15Width", (EXPRTYPE)glDriver.fbo[15].width);
	exprValListAdd(expression->v, "fbo15Height", (EXPRTYPE)glDriver.fbo[15].height);
	exprValListAdd(expression->v, "fbo16Width", (EXPRTYPE)glDriver.fbo[16].width);
	exprValListAdd(expression->v, "fbo16Height", (EXPRTYPE)glDriver.fbo[16].height);
	exprValListAdd(expression->v, "fbo17Width", (EXPRTYPE)glDriver.fbo[17].width);
	exprValListAdd(expression->v, "fbo17Height", (EXPRTYPE)glDriver.fbo[17].height);
	exprValListAdd(expression->v, "fbo18Width", (EXPRTYPE)glDriver.fbo[18].width);
	exprValListAdd(expression->v, "fbo18Height", (EXPRTYPE)glDriver.fbo[18].height);
	exprValListAdd(expression->v, "fbo19Width", (EXPRTYPE)glDriver.fbo[19].width);
	exprValListAdd(expression->v, "fbo19Height", (EXPRTYPE)glDriver.fbo[19].height);


	exprValListAdd(expression->v, "fbo20Width", (EXPRTYPE)glDriver.fbo[20].width);
	exprValListAdd(expression->v, "fbo20Height", (EXPRTYPE)glDriver.fbo[20].height);
	exprValListAdd(expression->v, "fbo21Width", (EXPRTYPE)glDriver.fbo[21].width);
	exprValListAdd(expression->v, "fbo21Height", (EXPRTYPE)glDriver.fbo[21].height);
	exprValListAdd(expression->v, "fbo22Width", (EXPRTYPE)glDriver.fbo[22].width);
	exprValListAdd(expression->v, "fbo22Height", (EXPRTYPE)glDriver.fbo[22].height);
	exprValListAdd(expression->v, "fbo23Width", (EXPRTYPE)glDriver.fbo[23].width);
	exprValListAdd(expression->v, "fbo23Height", (EXPRTYPE)glDriver.fbo[23].height);
	exprValListAdd(expression->v, "fbo24Width", (EXPRTYPE)glDriver.fbo[24].width);
	exprValListAdd(expression->v, "fbo24Height", (EXPRTYPE)glDriver.fbo[24].height);
}
