//
//  Basic video functions
//
//! \file tonc_oam.h
//! \author J Vijn
//! \date 20060604 - 20060604
//
// === NOTES ===
// * Basic video-IO, color, background and object functionality

#ifndef TONC_OAM
#define TONC_OAM

#include "tonc_memmap.h"
#include "tonc_memdef.h"
#include "tonc_core.h"


// --------------------------------------------------------------------
// OBJECTS
// --------------------------------------------------------------------


//! \addtogroup grpVideoObj
/*!	\{	*/

#define OAM_CLEAR()	memset32(oam_mem, 0, OAM_SIZE/4)

// --- Prototypes -----------------------------------------------------

// --- Full OAM ---
void oam_init(OBJ_ATTR *obj, uint count);
INLINE void oam_copy(OBJ_ATTR *dst, const OBJ_ATTR *src, uint count);

// --- Obj attr only ---
INLINE OBJ_ATTR *obj_set_attr(OBJ_ATTR *obj, u16 a0, u16 a1, u16 a2);
INLINE void obj_set_pos(OBJ_ATTR *obj, int x, int y);
INLINE void obj_hide(OBJ_ATTR *oatr);
INLINE void obj_unhide(OBJ_ATTR *obj, u16 mode);

INLINE const u8 *obj_get_size(const OBJ_ATTR *obj);
INLINE int obj_get_width(const OBJ_ATTR *obj);
INLINE int obj_get_height(const OBJ_ATTR *obj);

void obj_copy(OBJ_ATTR *dst, const OBJ_ATTR *src, uint count);
void obj_hide_multi(OBJ_ATTR *obj, uint count);
void obj_unhide_multi(OBJ_ATTR *obj, u16 mode, uint count);

/*!	\}	*/


// --------------------------------------------------------------------
// INLINES
// --------------------------------------------------------------------


/*!	\addtogroup grpVideoObj	*/
/*! \{	*/

//! Set the attributes of an object.
INLINE OBJ_ATTR *obj_set_attr(OBJ_ATTR *obj, u16 a0, u16 a1, u16 a2)
{
	obj->attr0= a0; obj->attr1= a1; obj->attr2= a2;
	return obj;
}

//! Set the position of \a obj
INLINE void obj_set_pos(OBJ_ATTR *obj, int x, int y)
{
	BFN_SET(obj->attr0, y, ATTR0_Y);
	BFN_SET(obj->attr1, x, ATTR1_X);
}

//! Copies \a count OAM entries from \a src to \a dst.
INLINE void oam_copy(OBJ_ATTR *dst, const OBJ_ATTR *src, uint count)
{	memcpy32(dst, src, count*2);	}

//! Hide an object.
INLINE void obj_hide(OBJ_ATTR *obj)
{	BFN_SET2(obj->attr0, ATTR0_HIDE, ATTR0_MODE);		}

//! Unhide an object.
/*! \param obj	Object to unhide.
*	\param mode	Object mode to unhide to. Necessary because this affects
*	  the affine-ness of the object.
*/
INLINE void obj_unhide(OBJ_ATTR *obj, u16 mode)
{	BFN_SET2(obj->attr0, mode, ATTR0_MODE);				}


//! Get object's sizes as a byte array
INLINE const u8 *obj_get_size(const OBJ_ATTR *obj)
{	return oam_sizes[obj->attr0>>14][obj->attr1>>14];	}

//! Get object's width
INLINE int obj_get_width(const OBJ_ATTR *obj)
{	return obj_get_size(obj)[0];						}
	
//! Gets object's height
INLINE int obj_get_height(const OBJ_ATTR *obj)
{	return obj_get_size(obj)[1];						}



#endif // TONC_OAM