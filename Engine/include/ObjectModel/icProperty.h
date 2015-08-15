#ifndef __IC_PROPERTY_H__
#define __IC_PROPERTY_H__

#include "Core/icGlobals.h"


template<class FieldType, class OwnerClassType>
class Property
{
private:
	friend OwnerClassType;

	typedef FieldType (OwnerClassType::*getMethodPtr)() const;
	typedef void (OwnerClassType::*setMethodPtr)(FieldType);

	OwnerClassType& _object;
	getMethodPtr     getMethod;
	setMethodPtr     setMethod;

public:

	Property(OwnerClassType& objInstance, getMethodPtr pmGet, setMethodPtr pmSet)
		:	_object(objInstance),
			getMethod(pmGet),
			setMethod(pmSet)
	{
	}
	
	FORCE_INLINE operator FieldType()
	{
		return (_object.*getMethod)();
	}

	FORCE_INLINE void operator =(FieldType value)
	{
		(_object.*setMethod)(value);
	}
};

template<class FieldType, class OwnerClassType>
class PrivateWriteProperty
{
private:
	typedef FieldType (OwnerClassType::*getMethodPtr)() const;
	typedef void (OwnerClassType::*setMethodPtr)(FieldType);

	OwnerClassType& _object;
	getMethodPtr     getMethod;
	setMethodPtr     setMethod;

protected:
	friend OwnerClassType;

	FORCE_INLINE void operator =(FieldType value)
	{
		(_object.*setMethod)(value);
	}

public:

	PrivateWriteProperty(OwnerClassType& objInstance, getMethodPtr pmGet, setMethodPtr pmSet)
		:	_object(objInstance),
			getMethod(pmGet),
			setMethod(pmSet)
	{
	}
	
	FORCE_INLINE operator FieldType()
	{
		return (_object.*getMethod)();
	}
};


#define FIELD(protection_level, name, type) \
protection_level: \
	type name; \

#endif //__IC_PROPERTY_H__
