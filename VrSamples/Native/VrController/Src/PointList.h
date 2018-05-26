/************************************************************************************

Filename    :   PointList.h
Content     :   Abstract base class for a list of points.
Created     :   6/16/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#pragma once

#include <cstdint>
#include <vector>
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Math.h"

namespace OVR {

//==============================================================
// ovrPointList
class ovrPointList
{
public:
	virtual ~ovrPointList()
	{
	}

	virtual bool 				IsEmpty() const = 0;
	virtual bool 				IsFull() const = 0;
	virtual int 				GetFirst() const = 0;
	virtual int 				GetNext( const int cur ) const = 0;
	virtual int 				GetLast() const = 0;
	virtual int 				GetCurPoints() const = 0;
	virtual int 				GetMaxPoints() const = 0;

	virtual const Vector3f & 	Get( const int index ) const = 0;
	virtual Vector3f & 			Get( const int index ) = 0;

	virtual void  				AddToTail( const Vector3f & p ) = 0;
	virtual void  				RemoveHead() = 0;
};

//==============================================================
// ovrPointList_Vector
class ovrPointList_Vector : public ovrPointList
{
public:
	ovrPointList_Vector( const int maxPoints );
	virtual ~ovrPointList_Vector() { }

	virtual bool 				IsEmpty() const OVR_OVERRIDE { return Points.size() == 0; }
	virtual bool 				IsFull() const OVR_OVERRIDE { return false; }
	virtual int 				GetFirst() const OVR_OVERRIDE { return 0; }
	virtual int					GetNext( const int cur ) const OVR_OVERRIDE;
	virtual int					GetLast() const OVR_OVERRIDE { return (int)Points.size() - 1; }
	virtual int 				GetCurPoints() const OVR_OVERRIDE { return Points.size(); }
	virtual int 				GetMaxPoints() const OVR_OVERRIDE { return MaxPoints; }

	virtual const Vector3f & 	Get( const int index ) const OVR_OVERRIDE { return Points[index]; }
	virtual Vector3f & 			Get( const int index ) OVR_OVERRIDE { return Points[index]; }

	virtual void  				AddToTail( const Vector3f & p ) OVR_OVERRIDE { Points.push_back( p ); }
	virtual void  				RemoveHead() OVR_OVERRIDE;
private:
	std::vector< Vector3f >	Points;
	int 					MaxPoints;
};

//==============================================================
// ovrPointList_Circular
class ovrPointList_Circular : public ovrPointList
{
public:
	ovrPointList_Circular( const int bufferSize );
	virtual ~ovrPointList_Circular() {	}

	virtual bool IsEmpty() const OVR_OVERRIDE { return HeadIndex == TailIndex; 	}
	virtual bool IsFull() const OVR_OVERRIDE { return IncIndex( TailIndex ) == HeadIndex; }
	virtual int GetFirst() const OVR_OVERRIDE;
	virtual int GetNext( const int cur ) const OVR_OVERRIDE;
	virtual int GetLast() const OVR_OVERRIDE;
	virtual int GetCurPoints() const OVR_OVERRIDE { return CurPoints; }
	virtual int GetMaxPoints() const OVR_OVERRIDE { return BufferSize - 1; 	}

	virtual const Vector3f & Get( const int index ) const OVR_OVERRIDE 	{ return Points[index];	}
	virtual Vector3f & Get( const int index ) OVR_OVERRIDE { return Points[index]; }

	virtual void  AddToTail( const Vector3f & p ) OVR_OVERRIDE;
	virtual void  RemoveHead() OVR_OVERRIDE;

private:
	std::vector< Vector3f >	Points;

	int 		BufferSize;
	int 		CurPoints;
	int 		HeadIndex;
	int 		TailIndex;

private:	
	int IncIndex( const int in ) const 	{ return ( in + 1 ) % BufferSize; }
	int DecIndex( const int in ) const;
};

} // namespace OVR
