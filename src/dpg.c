/****************************************************************************** **																			 ** ** 	Module:		dpg.c												 	 ** ** 																		 ** ** 																		 **																	 ** ** 																		 ** ** 	Copyright (C) 1996-1996 Apple Computer, Inc.  All rights reserved.	 ** ** 																		 ** ** 																		 ** *****************************************************************************/#include "DPGGroup.h"#include "QD3D.h"#include "QD3DCamera.h"#include "QD3DMath.h"#include "QD3DView.h"#include "QD3DExtension.h"#include "QD3DGroup.h"#include "dpg.h"#include "dpgIO.h"#include "dpgMemory.h"#include "dpgSortedArray.h"typedef struct TQ3DPGData{	TQ3Point3D	 	refPt;	unsigned long 	flag;} TQ3DPGData;static unsigned long EgSharedLibrary = NULL;OSErr  		DPGInitialize(const CFragInitBlock *initBlock);TQ3Status 	DPGExit(void);	/****************************************************************************** **		 **		Static Prototypes **		 *****************************************************************************/static TQ3XFunctionPointer exDistanceProxyGroup_MetaHandler(	TQ3XMethodType		methodType);static TQ3Boolean exDistanceProxyGroup_AcceptObject(	TQ3GroupObject		group,	TQ3Object			object);static TQ3GroupPosition exDistanceProxyGroup_AddObject(	TQ3GroupObject		group,	TQ3Object			object);	static TQ3GroupPosition exDistanceProxyGroup_AddObjectBefore(	TQ3GroupObject		group,	TQ3GroupPosition	position,	TQ3Object			object);	static TQ3GroupPosition exDistanceProxyGroup_AddObjectAfter(	TQ3GroupObject		group,	TQ3GroupPosition	position,	TQ3Object			object);static TQ3Status exDistanceProxyGroup_SetPositionObject(	TQ3GroupObject		group,	TQ3GroupPosition	position,	TQ3Object			object);	static TQ3Object exDistanceProxyGroup_RemovePosition(	TQ3GroupObject		group,	TQ3GroupPosition	position);static long exiDistanceProxyGroup_CompareDistance(	float				*key,	TQ3DistanceData		*data);	static TQ3Status exDistanceProxyGroup_AddDistance(	TQ3GroupObject							group,	TQ3DistanceProxyDisplayGroupPrivate		*gPriv,	TQ3GroupPosition						position,	float									distance);static TQ3Boolean exDistanceProxyGroup_Find(	TQ3GroupObject		group,	TQ3GroupPosition	position);static TQ3Status exDistanceProxyGroup_CalculateDistance(	TQ3ViewObject						view,	TQ3DistanceProxyDisplayGroupPrivate	*gPriv);static TQ3Boolean exDistanceProxyGroup_Visible(	TQ3DistanceProxyDisplayGroupPrivate	*gPriv,	TQ3GroupPosition					position);static TQ3Status exDistanceProxyGroup_ProcessIO(	TQ3GroupObject						group,	TQ3DistanceProxyDisplayGroupPrivate	*gPriv);/****************************************************************************** **		 **		Globals **		 *****************************************************************************/TQ3XObjectClass EgDistanceProxyDisplayGroupClass;/****************************************************************************** **		 **		DistanceProxy Group Routines **		 *****************************************************************************/static TQ3Status iDPG_New(	TQ3GroupObject							group,	TQ3DistanceProxyDisplayGroupPrivate		*gPriv,	const TQ3DPGData						*gData){	group; /* Unused */			gPriv->d			= 0.0;	gPriv->refPt		= gData->refPt;	gPriv->flag			= gData->flag;			gPriv->distTblCnt	= 0;	gPriv->distTbl		= NULL;	gPriv->distIOTbl	= 0;	gPriv->distIOTblCnt	= NULL;	return kQ3Success;}static TQ3Status iDPG_Copy(	TQ3GroupObject						fromGroup,	TQ3DistanceProxyDisplayGroupPrivate	*fromPriv,	TQ3GroupObject						toGroup,	TQ3DistanceProxyDisplayGroupPrivate	*toPriv){	register long i;		toPriv->refPt			= fromPriv->refPt;	toPriv->flag			= fromPriv->flag;	toPriv->position 		= NULL;		toPriv->distTbl			= NULL;	toPriv->distTblCnt		= 0;		toPriv->distIOTblCnt 	= 0;	toPriv->distIOTbl 		= dpgAlloc(sizeof(TQ3DistanceIOData)*fromPriv->distTblCnt);	if (!toPriv->distIOTbl) {		return kQ3Failure;	}		toPriv->distIOTblCnt 	= fromPriv->distTblCnt;		for (i=0;i < fromPriv->distTblCnt;i++) {		TQ3GroupPosition position;		register long index;				for (			Q3Group_GetFirstPosition(fromGroup, &position), index = 0; 			position != NULL; 			index++)		{			if (position == fromPriv->distTbl[i].position)			{				toPriv->distIOTbl[i].distance = fromPriv->distTbl[i].distance;				toPriv->distIOTbl[i].index    = index;				break;			}			Q3Group_GetNextPosition(fromGroup, &position);		}	}		return exDistanceProxyGroup_ProcessIO(toGroup, toPriv);	}static long exDistanceProxyGroup_CompareDistance(	float			*key,	TQ3DistanceData	*data){	float val = *key - data->distance;		if (fabs(val) < ((float)1.19209290e-07)) 		return 0;		if (val > 0.0)  		return  1;	return -1;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_Register() * *	Comments:	Registers the class. *\*===========================================================================*/TQ3Status exDistanceProxyGroup_Register(	void){	TQ3ObjectType objectType = kQ3DisplayGroupTypeDistanceProxy;		EgDistanceProxyDisplayGroupClass =		Q3XObjectHierarchy_RegisterClass(			kQ3GroupTypeDisplay,			&objectType, 			"Apple Computer Inc.:Display Group:DistanceProxyDisplayGroup",			exDistanceProxyGroup_MetaHandler,			NULL,			0,			sizeof(TQ3DistanceProxyDisplayGroupPrivate));		if (EgDistanceProxyDisplayGroupClass == NULL)		return kQ3Failure;		return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_New() * *	Comments:	Constructor *\*===========================================================================*/TQ3GroupObject exDistanceProxyGroup_New(	TQ3Point3D	 	*refPt,	unsigned long 	flag){	TQ3DPGData data;		data.refPt = *refPt;	data.flag  = flag;		return		Q3XObjectHierarchy_NewObject(			EgDistanceProxyDisplayGroupClass,			(void *) &data);}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_SetDistance() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_SetDistance(	TQ3GroupObject							group,	TQ3DistanceProxyDisplayGroupPrivate		*gPriv,	TQ3GroupPosition						position,	float									distance){	unsigned long		index;	TQ3DistanceData 	data;		data.position = position;	data.distance = distance;		/* set private position data */	{		TQ3DPGPositionPrivate *pPriv;		pPriv = DPG_POSITION_GETPRIVATE(group, position);		pPriv->flag 	= kQ3True;		pPriv->distance = distance;	}		/* insertion sort */		if (dpgSortedArray_Search(			(void *)&distance,			gPriv->distTbl,			gPriv->distTblCnt,			sizeof(TQ3DistanceData),			(dpgCompareFunction) exDistanceProxyGroup_CompareDistance,			&index) == kQ3True)	{		/* duplicate distance, replace with new one */		TQ3Object 			object;		TQ3GroupPosition 	oldPosition;				oldPosition = gPriv->distTbl[index].position;				/* call DefaultRemovePosition */		{			TQ3XGroupRemovePositionMethod 	method;			TQ3XObjectClass					objectClass;					objectClass = Q3XObjectHierarchy_FindClassByType(kQ3GroupTypeDisplay);					method = (TQ3XGroupRemovePositionMethod) Q3XObjectClass_GetMethod(														objectClass, 														kQ3XMethodType_GroupRemovePosition);			object = (*method)(group, oldPosition);		}				if(object)			Q3Object_Dispose(object);				gPriv->distTbl[index] = data;				return kQ3Success;	}	else {		/* insert new data */				gPriv->distTbl = dpgRealloc(							gPriv->distTbl, 							sizeof(TQ3DistanceData) * (gPriv->distTblCnt+1));		if (gPriv->distTbl == NULL)		{			return kQ3Failure;		}		dpgSortedArray_InsertElement(			gPriv->distTbl, 			gPriv->distTblCnt, 			sizeof(TQ3DistanceData),			&data,			index);					gPriv->distTblCnt++;	}	return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_AcceptObject() * *	Comments:	 *\*===========================================================================*/static TQ3Boolean exDistanceProxyGroup_AcceptObject(	TQ3GroupObject		group,	TQ3Object			object){		group; /* Unused */	return Q3Object_IsDrawable(object);}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_AddObjectDistance() * *	Comments:	 *\*===========================================================================*/TQ3GroupPosition exDistanceProxyGroup_AddObjectDistance(	TQ3GroupObject		group,	TQ3Object			object,	float				distance){	TQ3GroupPosition 					position;	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	gPriv = DPG_GETPRIVATE(group);	/*position = EiGroup_DefaultAddObject(group, object);*/	{		TQ3XGroupAddObjectMethod 	method;		TQ3XObjectClass				objectClass;				objectClass = Q3XObjectHierarchy_FindClassByType(kQ3GroupTypeDisplay);		method = (TQ3XGroupAddObjectMethod) Q3XObjectClass_GetMethod(												objectClass, 												kQ3XMethodType_GroupAddObject);		position = (*method)(group, object);	}		exDistanceProxyGroup_SetDistance(group, gPriv, position, distance);		return position;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_SetFlag() * *	Comments:	 *\*===========================================================================*/TQ3Status exDistanceProxyGroup_SetFlag(	TQ3GroupObject		group,	TQ3DPGFlag	 		flag){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	gPriv = DPG_GETPRIVATE(group);	gPriv->flag = flag;		return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_GetFlag() * *	Comments:	 *\*===========================================================================*/TQ3Status exDistanceProxyGroup_GetFlag(	TQ3GroupObject		group,	TQ3DPGFlag	 		*flag){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	gPriv = DPG_GETPRIVATE(group);	*flag = gPriv->flag;		return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_SetReferencePoint() * *	Comments:	 *\*===========================================================================*/TQ3Status exDistanceProxyGroup_SetReferencePoint(	TQ3GroupObject		group,	TQ3Point3D 			*refPt){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	gPriv = DPG_GETPRIVATE(group);	gPriv->refPt = *refPt;;		return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_GetReferencePoint() * *	Comments:	 *\*===========================================================================*/TQ3Status exDistanceProxyGroup_GetReferencePoint(	TQ3GroupObject		group,	TQ3Point3D 			*refPt){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	gPriv = DPG_GETPRIVATE(group);	*refPt = gPriv->refPt;		return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_SetDistanceAtPosition() * *	Comments:	 *\*===========================================================================*/TQ3Boolean exDistanceProxyGroup_SetDistanceAtPosition(	TQ3GroupObject		group,	TQ3GroupPosition	position,	float				distance){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	unsigned long 						index;	TQ3DPGPositionPrivate 				*pPriv;	gPriv = DPG_GETPRIVATE(group);		pPriv = DPG_POSITION_GETPRIVATE(group, position);	if (pPriv->flag) 	{				pPriv->distance = distance;				/* now fix up the distance table */		if (dpgSortedArray_Search(				(void *)&pPriv->distance,				gPriv->distTbl,				gPriv->distTblCnt,				sizeof(TQ3DistanceData),				(dpgCompareFunction) exDistanceProxyGroup_CompareDistance,				&index) == kQ3True)		{			/* found it, now get rid of it */			dpgSortedArray_DeleteElement(				gPriv->distTbl,				gPriv->distTblCnt,				sizeof(TQ3DistanceData),				NULL,				index);					gPriv->distTblCnt--;						if (dpgSortedArray_Search(					(void *)&distance,					gPriv->distTbl,					gPriv->distTblCnt,					sizeof(TQ3DistanceData),					(dpgCompareFunction) exDistanceProxyGroup_CompareDistance,					&index) == kQ3False)			{				TQ3DistanceData 	data;							data.position = position;				data.distance = distance;				dpgSortedArray_InsertElement(					gPriv->distTbl, 					gPriv->distTblCnt, 					sizeof(TQ3DistanceData),					&data,					index);			}								gPriv->distTblCnt++;		}				return kQ3True;	}	return kQ3False;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_GetDistanceAtPosition() * *	Comments:	 *\*===========================================================================*/TQ3Boolean exDistanceProxyGroup_GetDistanceAtPosition(	TQ3GroupObject		group,	TQ3GroupPosition	position,	float				*distance){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	TQ3DPGPositionPrivate 				*pPriv;	gPriv = DPG_GETPRIVATE(group);	pPriv = DPG_POSITION_GETPRIVATE(group, position);	*distance = 0.0;		if (pPriv->flag)	{		*distance = pPriv->distance;		return kQ3True;	}		return kQ3False;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_RemovePosition() * *	Comments:	 *\*===========================================================================*/static TQ3Object exDistanceProxyGroup_RemovePosition(	TQ3GroupObject		group,	TQ3GroupPosition	position){	unsigned long 	index;	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	TQ3DPGPositionPrivate 				*pPriv;	gPriv = DPG_GETPRIVATE(group);	pPriv = DPG_POSITION_GETPRIVATE(group, position);	if (pPriv->flag)	{		/* now remove distance entry */					if (dpgSortedArray_Search(			(void *)&pPriv->distance,			gPriv->distTbl,			gPriv->distTblCnt,			sizeof(TQ3DistanceData),			(dpgCompareFunction) exDistanceProxyGroup_CompareDistance,			&index) == kQ3True)		{			dpgSortedArray_DeleteElement(				gPriv->distTbl,				gPriv->distTblCnt,				sizeof(TQ3DistanceData),				NULL,				index);					gPriv->distTblCnt--;					dpgSortedArray_Resize(					(void **) &gPriv->distTbl,					gPriv->distTblCnt,					sizeof(TQ3DistanceData));		}									}	/* call DefaultRemovePosition */	{		TQ3XGroupRemovePositionMethod method;		TQ3XObjectClass				objectClass;				objectClass = Q3XObjectHierarchy_FindClassByType(kQ3GroupTypeDisplay);				method = (TQ3XGroupRemovePositionMethod) Q3XObjectClass_GetMethod(													objectClass, 													kQ3XMethodType_GroupRemovePosition);		return (*method)(group, position);	}}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_EndRead() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_EndRead(	TQ3GroupObject	group){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;		gPriv = DPG_GETPRIVATE(group);			exDistanceProxyGroup_ProcessIO(group, gPriv);	return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_StartState() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_StartState(	TQ3GroupObject	group,	TQ3ViewObject	view){	TQ3DisplayGroupState state;	Q3DisplayGroup_GetState(group, &state);		if ((state & kQ3DisplayGroupStateMaskIsInline) == 0)	{		if (Q3Push_Submit(view) == kQ3Failure)			return kQ3Failure;	}	return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_EndState() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_EndState(	TQ3GroupObject	group,	TQ3ViewObject	view){	TQ3DisplayGroupState state;	Q3DisplayGroup_GetState(group, &state);	if ((state & kQ3DisplayGroupStateMaskIsInline) == 0)	{		if (Q3Pop_Submit(view) == kQ3Failure)			return kQ3Failure;	}	return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_StartIterate() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_StartIterate(	TQ3GroupObject			group,	TQ3GroupPosition		*position,	TQ3Object				*object,	TQ3ViewObject			view){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	TQ3Status status;		gPriv = DPG_GETPRIVATE(group);	/* calculate current distance */	if (exDistanceProxyGroup_CalculateDistance(view, gPriv) == kQ3Failure)		return kQ3Failure;		*position = NULL;	*object	  = NULL;		status = Q3Group_GetFirstPosition(group, position);	/* find first visible object */			while ((status != kQ3Failure) &&			*position &&			exDistanceProxyGroup_Find(group, *position) == kQ3False) 	{		status = Q3Group_GetNextPosition(group, position);	}			if (status == kQ3Failure) {		*position = NULL;		return kQ3Failure;	}	/* nothing to draw */	if (*position == NULL)		return kQ3Success;		if (Q3Group_GetPositionObject(group, *position, object) == kQ3Failure)		return kQ3Failure;		if (exDistanceProxyGroup_StartState(group, view) == kQ3Failure)	{		Q3Object_Dispose(*object);		*object = NULL;		return kQ3Failure;	}	return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_EndIterate() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_EndIterate(	TQ3GroupObject			group,	TQ3GroupPosition		*position,	TQ3Object				*object,	TQ3ViewObject			view){	TQ3Status status;	if (*object != NULL)	{		Q3Object_Dispose(*object);		*object = NULL;		status = Q3Group_GetNextPosition(group, position);		while (	(status == kQ3Success) && 				*position && 				(exDistanceProxyGroup_Find(group, *position) == kQ3False)) 		{			status = Q3Group_GetNextPosition(group, position);		}				if (*position == NULL || status == kQ3Failure)		{			return exDistanceProxyGroup_EndState(group, view);		}			return Q3Group_GetPositionObject(group, *position, object);	}		*position = NULL;	return exDistanceProxyGroup_EndState(group, view);}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_CalculateDistance() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_CalculateDistance(	TQ3ViewObject						view,	TQ3DistanceProxyDisplayGroupPrivate	*gPriv){	TQ3CameraObject 	camera;	TQ3CameraPlacement	placement;	TQ3Matrix4x4		matrix;	TQ3Point3D			transPt;		Q3View_GetCamera (view, &camera);	if (!camera)		return kQ3Failure;			Q3Camera_GetPlacement(camera, &placement);	Q3Object_Dispose(camera);		if (Q3View_GetLocalToWorldMatrixState(view, &matrix) == kQ3Failure)		return kQ3Failure;			Q3Point3D_Transform(&gPriv->refPt, &matrix, &transPt);		/* found the distance */	gPriv->d = Q3Point3D_Distance(&placement.cameraLocation, &transPt);	/* now find the position to draw */		{			register long highElem	= gPriv->distTblCnt - 1;		register long midElem	= highElem / 2;		register long lowElem	= 0;		register long direction	= 1;			register TQ3DistanceData	*distTbl;				distTbl = gPriv->distTbl;		/* d >= MAX distance so use last object */		if (gPriv->d >= distTbl[highElem].distance && gPriv->flag != kQ3DPG_ClipWhenFar)		{			gPriv->position = distTbl[highElem].position;			return kQ3Success;		}		/* d < MIN distance so use first object */		if (gPriv->d < distTbl[0].distance && gPriv->flag != kQ3DPG_HideWhenNear)		{			gPriv->position = distTbl[0].position;			return kQ3Success;		}				/* d is in range, find the right object */		while (highElem >= lowElem)		{			direction = gPriv->d - distTbl[midElem].distance;						if ((direction == 0) ||				(gPriv->d >= distTbl[midElem-1].distance && 				 gPriv->d < distTbl[midElem].distance))			{				gPriv->position = distTbl[midElem].position;				return kQ3Success;			}							if (direction > 0)				lowElem  = midElem + 1;			else				highElem = midElem - 1;							midElem = (highElem + lowElem) >> 1;		}	}	return kQ3Failure;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_Find() * *	Comments:	 *\*===========================================================================*/static TQ3Boolean exDistanceProxyGroup_Find(	TQ3GroupObject			group,	TQ3GroupPosition		position){	TQ3DistanceProxyDisplayGroupPrivate	*gPriv;	TQ3DPGPositionPrivate 				*pPriv;	gPriv = DPG_GETPRIVATE(group);	pPriv = DPG_POSITION_GETPRIVATE(group, position);	if (pPriv->flag == kQ3False)	{		/* it's a object that doesn't have a distance 		** assume it want to be drawn 		*/		return kQ3True;	}		if (position == gPriv->position)		return kQ3True;	else		return kQ3False;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_ProcessIO() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_ProcessIO(	TQ3GroupObject						group,	TQ3DistanceProxyDisplayGroupPrivate	*gPriv){	if (gPriv->distIOTbl) 	{		TQ3GroupPosition position;		unsigned long index = 0;		unsigned long cnt   = 0;				for (Q3Group_GetFirstPosition(group, &position); position != NULL;cnt++)		{			if (gPriv->distIOTbl[index].index == cnt) 			{				if (exDistanceProxyGroup_SetDistance(					group, 					gPriv, 					position, 					gPriv->distIOTbl[index].distance) == kQ3Failure)				{					gPriv->distTblCnt = 0;					dpgFree(gPriv->distTbl);					gPriv->distTbl = NULL;					return kQ3Failure;				}				index++;			}			Q3Group_GetNextPosition(group, &position);		} 			dpgFree(gPriv->distIOTbl);				gPriv->distIOTbl 	= NULL;		gPriv->distIOTblCnt = 0;	}	return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_PositionNew() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_PositionNew(	TQ3DPGPositionPrivate	*gPos,	TQ3Object				object,	const TQ3Boolean		*initData){	object; /* XXX Unused */		if (initData == NULL)		gPos->flag = kQ3False;	else		gPos->flag = *initData;		gPos->distance = 0.0;		return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_PositionCopy() * *	Comments:	 *\*===========================================================================*/static TQ3Status exDistanceProxyGroup_PositionCopy(	TQ3DPGPositionPrivate	*srcGPos,	TQ3DPGPositionPrivate	*dstGPos){	*dstGPos = *srcGPos;	return kQ3Success;}/*===========================================================================*\ * *	Routine:	exDistanceProxyGroup_MetaHandler() * *	Comments:	 *\*===========================================================================*/static TQ3XFunctionPointer exDistanceProxyGroup_MetaHandler(	TQ3XMethodType		methodType){	switch (methodType) {		case kQ3XMethodTypeObjectNew:			return (TQ3XFunctionPointer) iDPG_New;		case kQ3XMethodTypeObjectDuplicate:			return (TQ3XFunctionPointer) iDPG_Copy;		case kQ3XMethodTypeObjectRead:			return (TQ3XFunctionPointer) exDistanceProxyGroup_Read;		case kQ3XMethodTypeObjectTraverse:			return (TQ3XFunctionPointer) exDistanceProxyGroup_Traverse;		case kQ3XMethodTypeObjectWrite:							return (TQ3XFunctionPointer) exDistanceProxyGroup_Write;			case kQ3XMethodType_GroupAcceptObject:			return (TQ3XFunctionPointer) exDistanceProxyGroup_AcceptObject;		case kQ3XMethodType_GroupRemovePosition:			return (TQ3XFunctionPointer) exDistanceProxyGroup_RemovePosition;				case kQ3XMethodType_GroupStartIterate:			return (TQ3XFunctionPointer) exDistanceProxyGroup_StartIterate;		case kQ3XMethodType_GroupEndIterate:			return (TQ3XFunctionPointer) exDistanceProxyGroup_EndIterate;			case kQ3XMethodType_GroupEndRead:			return (TQ3XFunctionPointer) exDistanceProxyGroup_EndRead;			case kQ3XMethodType_GroupPositionSize:			return (TQ3XFunctionPointer) 	sizeof(TQ3DPGPositionPrivate);		case kQ3XMethodType_GroupPositionNew:			return (TQ3XFunctionPointer) 	exDistanceProxyGroup_PositionNew;		case kQ3XMethodType_GroupPositionCopy:			return (TQ3XFunctionPointer) 	exDistanceProxyGroup_PositionCopy;				case kQ3XMethodTypeObjectIsDrawable:			return (TQ3XFunctionPointer) kQ3True;				default:			return (TQ3XFunctionPointer) NULL;	}}#if defined(OS_WIN32) && OS_WIN32	/*===========================================================================*\	 *	 *	Routine:	DllMain()	 *	 *	Comments:	The Win32 extension entry point	 *	\*===========================================================================*/	BOOL WINAPI _CRT_INIT(		HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);	BOOL WINAPI DllMain(		HINSTANCE	hinstDLL,		DWORD		fdwReason,		LPVOID		lpvReserved)	{		TQ3XSharedLibraryInfo	sharedLibraryInfo;				if ((fdwReason == DLL_PROCESS_ATTACH) || (fdwReason == DLL_THREAD_ATTACH))			if (!_CRT_INIT(hinstDLL, fdwReason, lpvReserved))				return (FALSE);		if (fdwReason == DLL_PROCESS_ATTACH)		{			sharedLibraryInfo.registerFunction = exDistanceProxyGroup_Register;			sharedLibraryInfo.sharedLibrary = (unsigned long)hinstDLL;			if (Q3XSharedLibrary_Register(&sharedLibraryInfo) == kQ3Success) 				return TRUE;			else				return FALSE;		}		else if (fdwReason == DLL_PROCESS_DETACH || fdwReason == DLL_THREAD_DETACH)			if (!_CRT_INIT(hinstDLL, fdwReason, lpvReserved))				return (FALSE);		if (fdwReason == DLL_PROCESS_DETACH)		{			Q3XSharedLibrary_Unregister( (unsigned long)hinstDLL);			return TRUE;		}		return (TRUE);	}#endif /* OS_WIN32 */#if defined(OS_MACINTOSH) && OS_MACINTOSHOSErr  DPGInitialize(const CFragInitBlock *	initBlock){	TQ3XSharedLibraryInfo	sharedLibraryInfo;			sharedLibraryInfo.registerFunction = exDistanceProxyGroup_Register;	sharedLibraryInfo.sharedLibrary = (unsigned long)initBlock->connectionID;	Q3XSharedLibrary_Register(&sharedLibraryInfo);		EgSharedLibrary = (unsigned long)initBlock->connectionID;	return noErr;}TQ3Status DPGExit(void){	if( EgSharedLibrary != NULL ) {		Q3XSharedLibrary_Unregister(EgSharedLibrary);		EgSharedLibrary = NULL;	}			return kQ3Success;}#endif#if 0/* outdated */long DPG_Init(	void){		if (!Q3IsInitialized()) {			if (Q3Initialize() == kQ3Failure)		{			return 1;		}	}		if (exDistanceProxyGroup_Register() == kQ3Failure)	{		return 1;	}	return 0;}long DPG_Exit(	void){		return 0;}#endif